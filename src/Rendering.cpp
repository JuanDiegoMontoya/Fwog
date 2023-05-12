#include <Fwog/Buffer.h>
#include <Fwog/Config.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Texture.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/detail/ContextState.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include FWOG_OPENGL_HEADER

// helper function
static void GLEnableOrDisable(GLenum state, GLboolean value)
{
  if (value)
    glEnable(state);
  else
    glDisable(state);
}

static size_t GetIndexSize(Fwog::IndexType indexType)
{
  switch (indexType)
  {
  case Fwog::IndexType::UNSIGNED_BYTE: return 1;
  case Fwog::IndexType::UNSIGNED_SHORT: return 2;
  case Fwog::IndexType::UNSIGNED_INT: return 4;
  default: FWOG_UNREACHABLE; return 0;
  }
}

static bool IsValidImageFormat(Fwog::Format format)
{
  switch (format)
  {
  case Fwog::Format::R32G32B32A32_FLOAT:
  case Fwog::Format::R16G16B16A16_FLOAT:
  case Fwog::Format::R32G32_FLOAT:
  case Fwog::Format::R16G16_FLOAT:
  case Fwog::Format::R11G11B10_FLOAT:
  case Fwog::Format::R32_FLOAT:
  case Fwog::Format::R16_FLOAT:
  case Fwog::Format::R32G32B32A32_UINT:
  case Fwog::Format::R16G16B16A16_UINT:
  case Fwog::Format::R10G10B10A2_UINT:
  case Fwog::Format::R8G8B8A8_UINT:
  case Fwog::Format::R32G32_UINT:
  case Fwog::Format::R16G16_UINT:
  case Fwog::Format::R8G8_UINT:
  case Fwog::Format::R32_UINT:
  case Fwog::Format::R16_UINT:
  case Fwog::Format::R8_UINT:
  case Fwog::Format::R32G32B32_SINT:
  case Fwog::Format::R16G16B16A16_SINT:
  case Fwog::Format::R8G8B8A8_SINT:
  case Fwog::Format::R32G32_SINT:
  case Fwog::Format::R16G16_SINT:
  case Fwog::Format::R8G8_SINT:
  case Fwog::Format::R32_SINT:
  case Fwog::Format::R16_SINT:
  case Fwog::Format::R8_SINT:
  case Fwog::Format::R16G16B16A16_UNORM:
  case Fwog::Format::R10G10B10A2_UNORM:
  case Fwog::Format::R8G8B8A8_UNORM:
  case Fwog::Format::R16G16_UNORM:
  case Fwog::Format::R8G8_UNORM:
  case Fwog::Format::R16_UNORM:
  case Fwog::Format::R8_UNORM:
  case Fwog::Format::R16G16B16A16_SNORM:
  case Fwog::Format::R8G8B8A8_SNORM:
  case Fwog::Format::R16G16_SNORM:
  case Fwog::Format::R8G8_SNORM:
  case Fwog::Format::R16_SNORM:
  case Fwog::Format::R8_SNORM: return true;
  default: return false;
  }
}

static bool IsDepthFormat(Fwog::Format format)
{
  switch (format)
  {
  case Fwog::Format::D32_FLOAT:
  case Fwog::Format::D32_UNORM:
  case Fwog::Format::D24_UNORM:
  case Fwog::Format::D16_UNORM:
  case Fwog::Format::D32_FLOAT_S8_UINT:
  case Fwog::Format::D24_UNORM_S8_UINT: return true;
  default: return false;
  }
}

static bool IsStencilFormat(Fwog::Format format)
{
  switch (format)
  {
  case Fwog::Format::D32_FLOAT_S8_UINT:
  case Fwog::Format::D24_UNORM_S8_UINT: return true;
  default: return false;
  }
}

static bool IsColorFormat(Fwog::Format format)
{
  return !IsDepthFormat(format) && !IsStencilFormat(format);
}

static uint32_t MakeSingleTextureFbo(const Fwog::Texture& texture, Fwog::detail::FramebufferCache& fboCache)
{
  auto format = texture.GetCreateInfo().format;

  auto depthStencil = Fwog::RenderDepthStencilAttachment{.texture = &texture};
  auto color = Fwog::RenderColorAttachment{.texture = &texture};
  Fwog::RenderInfo renderInfo;

  if (IsDepthFormat(format))
  {
    renderInfo.depthAttachment = &depthStencil;
  }

  if (IsStencilFormat(format))
  {
    renderInfo.stencilAttachment = &depthStencil;
  }

  if (IsColorFormat(format))
  {
    renderInfo.colorAttachments = {&color, 1};
  }

  return fboCache.CreateOrGetCachedFramebuffer(renderInfo);
}

static void SetViewportInternal(const Fwog::Viewport& viewport, const Fwog::Viewport& lastViewport, bool initViewport)
{
  if (initViewport || viewport.drawRect != lastViewport.drawRect)
  {
    glViewport(viewport.drawRect.offset.x,
               viewport.drawRect.offset.y,
               viewport.drawRect.extent.width,
               viewport.drawRect.extent.height);
  }
  if (initViewport || viewport.minDepth != lastViewport.minDepth || viewport.maxDepth != lastViewport.maxDepth)
  {
    glDepthRangef(viewport.minDepth, viewport.maxDepth);
  }
  if (initViewport || viewport.depthRange != lastViewport.depthRange)
  {
    glClipControl(GL_LOWER_LEFT, Fwog::detail::DepthRangeToGL(viewport.depthRange));
  }
}

namespace Fwog
{
  using namespace Fwog::detail;

  void BeginSwapchainRendering(const SwapchainRenderInfo& renderInfo)
  {
    FWOG_ASSERT(!context->isRendering && "Cannot call BeginRendering when rendering");
    FWOG_ASSERT(!context->isComputeActive && "Cannot nest compute and rendering");
    context->isRendering = true;
    context->isRenderingToSwapchain = true;
    context->lastRenderInfo = nullptr;

#ifdef FWOG_DEBUG
    detail::ZeroResourceBindings();
#endif

    const auto& ri = renderInfo;

    if (!ri.name.empty())
    {
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>(ri.name.size()), ri.name.data());
      context->isScopedDebugGroupPushed = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    switch (ri.colorLoadOp)
    {
    case AttachmentLoadOp::LOAD: break;
    case AttachmentLoadOp::CLEAR:
    {
      FWOG_ASSERT((std::holds_alternative<std::array<float, 4>>(ri.clearColorValue.data)));
      if (context->lastColorMask[0] != ColorComponentFlag::RGBA_BITS)
      {
        glColorMaski(0, true, true, true, true);
        context->lastColorMask[0] = ColorComponentFlag::RGBA_BITS;
      }
      glClearNamedFramebufferfv(0, GL_COLOR, 0, std::get_if<std::array<float, 4>>(&ri.clearColorValue.data)->data());
      break;
    }
    case AttachmentLoadOp::DONT_CARE:
    {
      GLenum attachment = GL_COLOR;
      glInvalidateNamedFramebufferData(0, 1, &attachment);
      break;
    }
    default: FWOG_UNREACHABLE;
    }

    switch (ri.depthLoadOp)
    {
    case AttachmentLoadOp::LOAD: break;
    case AttachmentLoadOp::CLEAR:
    {
      if (context->lastDepthMask == false)
      {
        glDepthMask(true);
        context->lastDepthMask = true;
      }
      glClearNamedFramebufferfv(0, GL_DEPTH, 0, &ri.clearDepthValue);
      break;
    }
    case AttachmentLoadOp::DONT_CARE:
    {
      GLenum attachment = GL_DEPTH;
      glInvalidateNamedFramebufferData(0, 1, &attachment);
      break;
    }
    default: FWOG_UNREACHABLE;
    }

    switch (ri.stencilLoadOp)
    {
    case AttachmentLoadOp::LOAD: break;
    case AttachmentLoadOp::CLEAR:
    {
      if (context->lastStencilMask[0] == false || context->lastStencilMask[1] == false)
      {
        glStencilMask(true);
        context->lastStencilMask[0] = true;
        context->lastStencilMask[1] = true;
      }
      glClearNamedFramebufferiv(0, GL_STENCIL, 0, &ri.clearStencilValue);
      break;
    }
    case AttachmentLoadOp::DONT_CARE:
    {
      GLenum attachment = GL_STENCIL;
      glInvalidateNamedFramebufferData(0, 1, &attachment);
      break;
    }
    default: FWOG_UNREACHABLE;
    }

    // Framebuffer sRGB can only be disabled in this exact function
    if (!renderInfo.enableSrgb)
    {
      glDisable(GL_FRAMEBUFFER_SRGB);
      context->srgbWasDisabled = true;
    }

    SetViewportInternal(renderInfo.viewport, context->lastViewport, context->initViewport);

    context->lastViewport = renderInfo.viewport;
    context->initViewport = false;
  }

  void BeginRendering(const RenderInfo& renderInfo)
  {
    FWOG_ASSERT(!context->isRendering && "Cannot call BeginRendering when rendering");
    FWOG_ASSERT(!context->isComputeActive && "Cannot nest compute and rendering");
    context->isRendering = true;

#ifdef FWOG_DEBUG
    detail::ZeroResourceBindings();
#endif

    // if (lastRenderInfo == &renderInfo)
    //{
    //   return;
    // }

    context->lastRenderInfo = &renderInfo;

    const auto& ri = renderInfo;

    if (!ri.name.empty())
    {
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>(ri.name.size()), ri.name.data());
      context->isScopedDebugGroupPushed = true;
    }

    context->currentFbo = context->fboCache.CreateOrGetCachedFramebuffer(ri);
    glBindFramebuffer(GL_FRAMEBUFFER, context->currentFbo);

    for (GLint i = 0; i < static_cast<GLint>(ri.colorAttachments.size()); i++)
    {
      const auto& attachment = ri.colorAttachments[i];
      switch (attachment.loadOp)
      {
      case AttachmentLoadOp::LOAD: break;
      case AttachmentLoadOp::CLEAR:
      {
        if (context->lastColorMask[i] != ColorComponentFlag::RGBA_BITS)
        {
          glColorMaski(i, true, true, true, true);
          context->lastColorMask[i] = ColorComponentFlag::RGBA_BITS;
        }

        auto format = attachment.texture->GetCreateInfo().format;
        auto baseTypeClass = detail::FormatToBaseTypeClass(format);

        auto& ccv = attachment.clearValue;

        switch (baseTypeClass)
        {
        case detail::GlBaseTypeClass::FLOAT:
          FWOG_ASSERT((std::holds_alternative<std::array<float, 4>>(ccv.data)));
          glClearNamedFramebufferfv(context->currentFbo, GL_COLOR, i, std::get_if<std::array<float, 4>>(&ccv.data)->data());
          break;
        case detail::GlBaseTypeClass::SINT:
          FWOG_ASSERT((std::holds_alternative<std::array<int32_t, 4>>(ccv.data)));
          glClearNamedFramebufferiv(context->currentFbo, GL_COLOR, i, std::get_if<std::array<int32_t, 4>>(&ccv.data)->data());
          break;
        case detail::GlBaseTypeClass::UINT:
          FWOG_ASSERT((std::holds_alternative<std::array<uint32_t, 4>>(ccv.data)));
          glClearNamedFramebufferuiv(context->currentFbo,
                                     GL_COLOR,
                                     i,
                                     std::get_if<std::array<uint32_t, 4>>(&ccv.data)->data());
          break;
        default: FWOG_UNREACHABLE;
        }
        break;
      }
      case AttachmentLoadOp::DONT_CARE:
      {
        GLenum colorAttachment = GL_COLOR_ATTACHMENT0 + i;
        glInvalidateNamedFramebufferData(context->currentFbo, 1, &colorAttachment);
        break;
      }
      default: FWOG_UNREACHABLE;
      }
    }

    if (ri.depthAttachment)
    {
      switch (ri.depthAttachment->loadOp)
      {
      case AttachmentLoadOp::LOAD: break;
      case AttachmentLoadOp::CLEAR:
      {
        // clear just depth
        if (context->lastDepthMask == false)
        {
          glDepthMask(true);
          context->lastDepthMask = true;
        }

        glClearNamedFramebufferfv(context->currentFbo, GL_DEPTH, 0, &ri.depthAttachment->clearValue.depth);
        break;
      }
      case AttachmentLoadOp::DONT_CARE:
      {
        GLenum attachment = GL_DEPTH_ATTACHMENT;
        glInvalidateNamedFramebufferData(context->currentFbo, 1, &attachment);
        break;
      }
      default: FWOG_UNREACHABLE;
      }
    }

    if (ri.stencilAttachment)
    {
      switch (ri.stencilAttachment->loadOp)
      {
      case AttachmentLoadOp::LOAD: break;
      case AttachmentLoadOp::CLEAR:
      {
        // clear just stencil
        if (context->lastStencilMask[0] == false || context->lastStencilMask[1] == false)
        {
          glStencilMask(true);
          context->lastStencilMask[0] = true;
          context->lastStencilMask[1] = true;
        }

        glClearNamedFramebufferiv(context->currentFbo, GL_STENCIL, 0, &ri.stencilAttachment->clearValue.stencil);
        break;
      }
      case AttachmentLoadOp::DONT_CARE:
      {
        GLenum attachment = GL_STENCIL_ATTACHMENT;
        glInvalidateNamedFramebufferData(context->currentFbo, 1, &attachment);
        break;
      }
      default: FWOG_UNREACHABLE;
      }
    }

    Viewport viewport{};
    if (ri.viewport)
    {
      viewport = *ri.viewport;
    }
    else
    {
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      // determine intersection of all render targets
      Rect2D drawRect{.offset = {}, .extent = {std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()}};
      for (const auto& attachment : ri.colorAttachments)
      {
        drawRect.extent.width = std::min(drawRect.extent.width, attachment.texture->GetCreateInfo().extent.width);
        drawRect.extent.height = std::min(drawRect.extent.height, attachment.texture->GetCreateInfo().extent.height);
      }
      if (ri.depthAttachment)
      {
        drawRect.extent.width = std::min(drawRect.extent.width, ri.depthAttachment->texture->GetCreateInfo().extent.width);
        drawRect.extent.height =
          std::min(drawRect.extent.height, ri.depthAttachment->texture->GetCreateInfo().extent.height);
      }
      if (ri.stencilAttachment)
      {
        drawRect.extent.width =
          std::min(drawRect.extent.width, ri.stencilAttachment->texture->GetCreateInfo().extent.width);
        drawRect.extent.height =
          std::min(drawRect.extent.height, ri.stencilAttachment->texture->GetCreateInfo().extent.height);
      }
      viewport.drawRect = drawRect;
    }

    SetViewportInternal(viewport, context->lastViewport, context->initViewport);

    context->lastViewport = viewport;
    context->initViewport = false;
  }

  void EndRendering()
  {
    FWOG_ASSERT(context->isRendering && "Cannot call EndRendering when not rendering");
    context->isRendering = false;
    context->isIndexBufferBound = false;
    context->isRenderingToSwapchain = false;

    if (context->isScopedDebugGroupPushed)
    {
      context->isScopedDebugGroupPushed = false;
      glPopDebugGroup();
    }

    if (context->isPipelineDebugGroupPushed)
    {
      context->isPipelineDebugGroupPushed = false;
      glPopDebugGroup();
    }

    if (context->scissorEnabled)
    {
      glDisable(GL_SCISSOR_TEST);
      context->scissorEnabled = false;
    }

    if (context->srgbWasDisabled)
    {
      glEnable(GL_FRAMEBUFFER_SRGB);
    }
  }

  void BeginCompute(std::string_view name)
  {
    FWOG_ASSERT(!context->isComputeActive);
    FWOG_ASSERT(!context->isRendering && "Cannot nest compute and rendering");
    context->isComputeActive = true;

#ifdef FWOG_DEBUG
    detail::ZeroResourceBindings();
#endif

    if (!name.empty())
    {
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>(name.size()), name.data());
      context->isScopedDebugGroupPushed = true;
    }
  }

  void EndCompute()
  {
    FWOG_ASSERT(context->isComputeActive);
    context->isComputeActive = false;

    if (context->isScopedDebugGroupPushed)
    {
      context->isScopedDebugGroupPushed = false;
      glPopDebugGroup();
    }

    if (context->isPipelineDebugGroupPushed)
    {
      context->isPipelineDebugGroupPushed = false;
      glPopDebugGroup();
    }
  }

  void BlitTexture(const Texture& source,
                   const Texture& target,
                   Offset3D sourceOffset,
                   Offset3D targetOffset,
                   Extent3D sourceExtent,
                   Extent3D targetExtent,
                   Filter filter,
                   AspectMask aspect)
  {
    auto fboSource = MakeSingleTextureFbo(source, context->fboCache);
    auto fboTarget = MakeSingleTextureFbo(target, context->fboCache);
    glBlitNamedFramebuffer(fboSource,
                           fboTarget,
                           sourceOffset.x,
                           sourceOffset.y,
                           sourceExtent.width,
                           sourceExtent.height,
                           targetOffset.x,
                           targetOffset.y,
                           targetExtent.width,
                           targetExtent.height,
                           detail::AspectMaskToGL(aspect),
                           detail::FilterToGL(filter));
  }

  void BlitTextureToSwapchain(const Texture& source,
                              Offset3D sourceOffset,
                              Offset3D targetOffset,
                              Extent3D sourceExtent,
                              Extent3D targetExtent,
                              Filter filter,
                              AspectMask aspect)
  {
    auto fbo = MakeSingleTextureFbo(source, context->fboCache);

    glBlitNamedFramebuffer(fbo,
                           0,
                           sourceOffset.x,
                           sourceOffset.y,
                           sourceExtent.width,
                           sourceExtent.height,
                           targetOffset.x,
                           targetOffset.y,
                           targetExtent.width,
                           targetExtent.height,
                           detail::AspectMaskToGL(aspect),
                           detail::FilterToGL(filter));
  }

  void CopyTexture(const CopyTextureInfo& copy)
  {
    glCopyImageSubData(detail::GetHandle(copy.source),
                       GL_TEXTURE,
                       copy.sourceLevel,
                       copy.sourceOffset.x,
                       copy.sourceOffset.y,
                       copy.sourceOffset.z,
                       copy.target.Handle(),
                       GL_TEXTURE,
                       copy.targetLevel,
                       copy.targetOffset.x,
                       copy.targetOffset.y,
                       copy.targetOffset.z,
                       copy.extent.width,
                       copy.extent.height,
                       copy.extent.depth);
  }

  void MemoryBarrier(MemoryBarrierBits accessBits)
  {
    glMemoryBarrier(detail::BarrierBitsToGL(accessBits));
  }

  void TextureBarrier()
  {
    glTextureBarrier();
  }

  void CopyBuffer(const CopyBufferInfo& copy)
  {
    auto size = copy.size;
    if (size == WHOLE_BUFFER)
    {
      size = copy.source.Size() - copy.sourceOffset;
    }

    glCopyNamedBufferSubData(copy.source.Handle(),
                             copy.target.Handle(),
                             static_cast<GLintptr>(copy.sourceOffset),
                             static_cast<GLintptr>(copy.targetOffset),
                             static_cast<GLsizeiptr>(size));
  }

  void CopyTextureToBuffer(const CopyTextureToBufferInfo& copy)
  {
    glPixelStorei(GL_PACK_ROW_LENGTH, copy.bufferRowLength);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, copy.bufferImageHeight);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, copy.targetBuffer.Handle());

    GLenum format{};
    if (copy.format == UploadFormat::INFER_FORMAT)
    {
      format = detail::UploadFormatToGL(detail::FormatToUploadFormat(copy.sourceTexture.GetCreateInfo().format));
    }
    else
    {
      format = detail::UploadFormatToGL(copy.format);
    }

    GLenum type{};
    if (copy.type == UploadType::INFER_TYPE)
    {
      type = detail::FormatToTypeGL(copy.sourceTexture.GetCreateInfo().format);
    }
    else
    {
      type = detail::UploadTypeToGL(copy.type);
    }

    glGetTextureSubImage(const_cast<Texture&>(copy.sourceTexture).Handle(),
                         copy.level,
                         copy.sourceOffset.x,
                         copy.sourceOffset.z,
                         copy.sourceOffset.z,
                         copy.extent.width,
                         copy.extent.height,
                         copy.extent.depth,
                         format,
                         type,
                         static_cast<GLsizei>(copy.targetBuffer.Size()),
                         reinterpret_cast<void*>(static_cast<uintptr_t>(copy.targetOffset)));
  }

  void CopyBufferToTexture(const CopyBufferToTextureInfo& copy)
  {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, copy.bufferRowLength);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, copy.bufferImageHeight);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, copy.sourceBuffer.Handle());

    const_cast<Texture&>(copy.targetTexture)
      .subImageInternal({copy.level,
                         copy.targetOffset,
                         copy.extent,
                         copy.format,
                         copy.type,
                         reinterpret_cast<void*>(static_cast<uintptr_t>(copy.sourceOffset)),
                         copy.bufferRowLength,
                         copy.bufferImageHeight});
  }

  namespace Cmd
  {
    void BindGraphicsPipeline(const GraphicsPipeline& pipeline)
    {
      FWOG_ASSERT(context->isRendering);
      FWOG_ASSERT(pipeline.Handle() != 0);

      auto pipelineState = detail::GetGraphicsPipelineInternal(pipeline.Handle());
      FWOG_ASSERT(pipelineState);

      if (context->lastGraphicsPipeline == pipelineState)
      {
        return;
      }

      if (context->isPipelineDebugGroupPushed)
      {
        context->isPipelineDebugGroupPushed = false;
        glPopDebugGroup();
      }

      if (!pipelineState->name.empty())
      {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION,
                         0,
                         static_cast<GLsizei>(pipelineState->name.size()),
                         pipelineState->name.data());
        context->isPipelineDebugGroupPushed = true;
      }

      // Always enable this.
      // The user can create a context with a non-sRGB framebuffer or create a non-sRGB view of an sRGB texture.
      if (!context->lastGraphicsPipeline)
      {
        glEnable(GL_FRAMEBUFFER_SRGB);
      }

      //////////////////////////////////////////////////////////////// shader program
      glUseProgram(static_cast<GLuint>(pipeline.Handle()));

      //////////////////////////////////////////////////////////////// input assembly
      const auto& ias = pipelineState->inputAssemblyState;
      if (!context->lastGraphicsPipeline ||
          ias.primitiveRestartEnable != context->lastGraphicsPipeline->inputAssemblyState.primitiveRestartEnable)
      {
        GLEnableOrDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX, ias.primitiveRestartEnable);
      }
      context->currentTopology = ias.topology;

      //////////////////////////////////////////////////////////////// vertex input
      if (auto nextVao = context->vaoCache.CreateOrGetCachedVertexArray(pipelineState->vertexInputState);
          nextVao != context->currentVao)
      {
        context->currentVao = nextVao;
        glBindVertexArray(context->currentVao);
      }

      //////////////////////////////////////////////////////////////// tessellation
      const auto& ts = pipelineState->tessellationState;
      if (ts.patchControlPoints > 0)
      {
        if (!context->lastGraphicsPipeline ||
            ts.patchControlPoints != context->lastGraphicsPipeline->tessellationState.patchControlPoints)
        {
          glPatchParameteri(GL_PATCH_VERTICES, static_cast<GLint>(pipelineState->tessellationState.patchControlPoints));
        }
      }

      //////////////////////////////////////////////////////////////// rasterization
      const auto& rs = pipelineState->rasterizationState;
      if (!context->lastGraphicsPipeline ||
          rs.depthClampEnable != context->lastGraphicsPipeline->rasterizationState.depthClampEnable)
      {
        GLEnableOrDisable(GL_DEPTH_CLAMP, rs.depthClampEnable);
      }

      if (!context->lastGraphicsPipeline || rs.polygonMode != context->lastGraphicsPipeline->rasterizationState.polygonMode)
      {
        glPolygonMode(GL_FRONT_AND_BACK, detail::PolygonModeToGL(rs.polygonMode));
      }

      if (!context->lastGraphicsPipeline || rs.cullMode != context->lastGraphicsPipeline->rasterizationState.cullMode)
      {
        GLEnableOrDisable(GL_CULL_FACE, rs.cullMode != CullMode::NONE);
        if (rs.cullMode != CullMode::NONE)
        {
          glCullFace(detail::CullModeToGL(rs.cullMode));
        }
      }

      if (!context->lastGraphicsPipeline || rs.frontFace != context->lastGraphicsPipeline->rasterizationState.frontFace)
      {
        glFrontFace(detail::FrontFaceToGL(rs.frontFace));
      }

      if (!context->lastGraphicsPipeline ||
          rs.depthBiasEnable != context->lastGraphicsPipeline->rasterizationState.depthBiasEnable)
      {
        GLEnableOrDisable(GL_POLYGON_OFFSET_FILL, rs.depthBiasEnable);
        GLEnableOrDisable(GL_POLYGON_OFFSET_LINE, rs.depthBiasEnable);
        GLEnableOrDisable(GL_POLYGON_OFFSET_POINT, rs.depthBiasEnable);
      }

      if (!context->lastGraphicsPipeline ||
          rs.depthBiasSlopeFactor != context->lastGraphicsPipeline->rasterizationState.depthBiasSlopeFactor ||
          rs.depthBiasConstantFactor != context->lastGraphicsPipeline->rasterizationState.depthBiasConstantFactor)
      {
        glPolygonOffset(rs.depthBiasSlopeFactor, rs.depthBiasConstantFactor);
      }

      if (!context->lastGraphicsPipeline || rs.lineWidth != context->lastGraphicsPipeline->rasterizationState.lineWidth)
      {
        glLineWidth(rs.lineWidth);
      }

      if (!context->lastGraphicsPipeline || rs.pointSize != context->lastGraphicsPipeline->rasterizationState.pointSize)
      {
        glPointSize(rs.pointSize);
      }

      //////////////////////////////////////////////////////////////// multisample
      const auto& ms = pipelineState->multisampleState;
      if (!context->lastGraphicsPipeline ||
          ms.sampleShadingEnable != context->lastGraphicsPipeline->multisampleState.sampleShadingEnable)
      {
        GLEnableOrDisable(GL_SAMPLE_SHADING, ms.sampleShadingEnable);
      }

      if (!context->lastGraphicsPipeline ||
          ms.minSampleShading != context->lastGraphicsPipeline->multisampleState.minSampleShading)
      {
        glMinSampleShading(ms.minSampleShading);
      }

      if (!context->lastGraphicsPipeline || ms.sampleMask != context->lastGraphicsPipeline->multisampleState.sampleMask)
      {
        GLEnableOrDisable(GL_SAMPLE_MASK, ms.sampleMask != 0xFFFFFFFF);
        glSampleMaski(0, ms.sampleMask);
      }

      if (!context->lastGraphicsPipeline ||
          ms.alphaToCoverageEnable != context->lastGraphicsPipeline->multisampleState.alphaToCoverageEnable)
      {
        GLEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, ms.alphaToCoverageEnable);
      }

      if (!context->lastGraphicsPipeline ||
          ms.alphaToOneEnable != context->lastGraphicsPipeline->multisampleState.alphaToOneEnable)
      {
        GLEnableOrDisable(GL_SAMPLE_ALPHA_TO_ONE, ms.alphaToOneEnable);
      }

      //////////////////////////////////////////////////////////////// depth + stencil
      const auto& ds = pipelineState->depthState;
      if (!context->lastGraphicsPipeline || ds.depthTestEnable != context->lastGraphicsPipeline->depthState.depthTestEnable)
      {
        GLEnableOrDisable(GL_DEPTH_TEST, ds.depthTestEnable);
      }

      if (ds.depthTestEnable)
      {
        if (!context->lastGraphicsPipeline ||
            ds.depthWriteEnable != context->lastGraphicsPipeline->depthState.depthWriteEnable)
        {
          if (ds.depthWriteEnable != context->lastDepthMask)
          {
            glDepthMask(ds.depthWriteEnable);
            context->lastDepthMask = ds.depthWriteEnable;
          }
        }

        if (!context->lastGraphicsPipeline || ds.depthCompareOp != context->lastGraphicsPipeline->depthState.depthCompareOp)
        {
          glDepthFunc(detail::CompareOpToGL(ds.depthCompareOp));
        }
      }

      const auto& ss = pipelineState->stencilState;
      if (!context->lastGraphicsPipeline ||
          ss.stencilTestEnable != context->lastGraphicsPipeline->stencilState.stencilTestEnable)
      {
        GLEnableOrDisable(GL_STENCIL_TEST, ss.stencilTestEnable);
      }

      if (ss.stencilTestEnable)
      {
        if (!context->lastGraphicsPipeline || !context->lastGraphicsPipeline->stencilState.stencilTestEnable ||
            ss.front != context->lastGraphicsPipeline->stencilState.front)
        {
          glStencilOpSeparate(GL_FRONT,
                              detail::StencilOpToGL(ss.front.failOp),
                              detail::StencilOpToGL(ss.front.depthFailOp),
                              detail::StencilOpToGL(ss.front.passOp));
          glStencilFuncSeparate(GL_FRONT, detail::CompareOpToGL(ss.front.compareOp), ss.front.reference, ss.front.compareMask);
          if (context->lastStencilMask[0] != ss.front.writeMask)
          {
            glStencilMaskSeparate(GL_FRONT, ss.front.writeMask);
            context->lastStencilMask[0] = ss.front.writeMask;
          }
        }

        if (!context->lastGraphicsPipeline || !context->lastGraphicsPipeline->stencilState.stencilTestEnable ||
            ss.back != context->lastGraphicsPipeline->stencilState.back)
        {
          glStencilOpSeparate(GL_BACK,
                              detail::StencilOpToGL(ss.back.failOp),
                              detail::StencilOpToGL(ss.back.depthFailOp),
                              detail::StencilOpToGL(ss.back.passOp));
          glStencilFuncSeparate(GL_BACK, detail::CompareOpToGL(ss.back.compareOp), ss.back.reference, ss.back.compareMask);
          if (context->lastStencilMask[1] != ss.back.writeMask)
          {
            glStencilMaskSeparate(GL_BACK, ss.back.writeMask);
            context->lastStencilMask[1] = ss.back.writeMask;
          }
        }
      }

      //////////////////////////////////////////////////////////////// color blending state
      const auto& cb = pipelineState->colorBlendState;
      if (!context->lastGraphicsPipeline || cb.logicOpEnable != context->lastGraphicsPipeline->colorBlendState.logicOpEnable)
      {
        GLEnableOrDisable(GL_COLOR_LOGIC_OP, cb.logicOpEnable);
        if (!context->lastGraphicsPipeline || !context->lastGraphicsPipeline->colorBlendState.logicOpEnable ||
            (cb.logicOpEnable && cb.logicOp != context->lastGraphicsPipeline->colorBlendState.logicOp))
        {
          glLogicOp(detail::LogicOpToGL(cb.logicOp));
        }
      }

      if (!context->lastGraphicsPipeline || std::memcmp(cb.blendConstants,
                                                        context->lastGraphicsPipeline->colorBlendState.blendConstants,
                                                        sizeof(cb.blendConstants)) != 0)
      {
        glBlendColor(cb.blendConstants[0], cb.blendConstants[1], cb.blendConstants[2], cb.blendConstants[3]);
      }

      // FWOG_ASSERT((cb.attachments.empty()
      //   || (isRenderingToSwapchain && !cb.attachments.empty()))
      //   || lastRenderInfo->colorAttachments.size() >= cb.attachments.size()
      //   && "There must be at least a color blend attachment for each render target, or none");

      if (!context->lastGraphicsPipeline ||
          cb.attachments.empty() != context->lastGraphicsPipeline->colorBlendState.attachments.empty())
      {
        GLEnableOrDisable(GL_BLEND, !cb.attachments.empty());
      }

      for (GLuint i = 0; i < static_cast<GLuint>(cb.attachments.size()); i++)
      {
        const auto& cba = cb.attachments[i];
        if (context->lastGraphicsPipeline && i < context->lastGraphicsPipeline->colorBlendState.attachments.size() &&
            cba == context->lastGraphicsPipeline->colorBlendState.attachments[i])
        {
          continue;
        }

        if (cba.blendEnable)
        {
          glBlendFuncSeparatei(i,
                               detail::BlendFactorToGL(cba.srcColorBlendFactor),
                               detail::BlendFactorToGL(cba.dstColorBlendFactor),
                               detail::BlendFactorToGL(cba.srcAlphaBlendFactor),
                               detail::BlendFactorToGL(cba.dstAlphaBlendFactor));
          glBlendEquationSeparatei(i, detail::BlendOpToGL(cba.colorBlendOp), detail::BlendOpToGL(cba.alphaBlendOp));
        }
        else
        {
          // "no blending" blend state
          glBlendFuncSeparatei(i, GL_SRC_COLOR, GL_ZERO, GL_SRC_ALPHA, GL_ZERO);
          glBlendEquationSeparatei(i, GL_FUNC_ADD, GL_FUNC_ADD);
        }

        if (context->lastColorMask[i] != cba.colorWriteMask)
        {
          glColorMaski(i,
                       (cba.colorWriteMask & ColorComponentFlag::R_BIT) != ColorComponentFlag::NONE,
                       (cba.colorWriteMask & ColorComponentFlag::G_BIT) != ColorComponentFlag::NONE,
                       (cba.colorWriteMask & ColorComponentFlag::B_BIT) != ColorComponentFlag::NONE,
                       (cba.colorWriteMask & ColorComponentFlag::A_BIT) != ColorComponentFlag::NONE);
          context->lastColorMask[i] = cba.colorWriteMask;
        }
      }

      context->lastGraphicsPipeline = pipelineState;
    }

    void BindComputePipeline(const ComputePipeline& pipeline)
    {
      FWOG_ASSERT(context->isComputeActive);
      FWOG_ASSERT(pipeline.Handle() != 0);

      auto pipelineState = detail::GetComputePipelineInternal(pipeline.Handle());

      context->lastComputePipelineWorkgroupSize = pipeline.WorkgroupSize();

      if (context->isPipelineDebugGroupPushed)
      {
        context->isPipelineDebugGroupPushed = false;
        glPopDebugGroup();
      }

      if (!pipelineState->name.empty())
      {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION,
                         0,
                         static_cast<GLsizei>(pipelineState->name.size()),
                         pipelineState->name.data());
        context->isPipelineDebugGroupPushed = true;
      }

      glUseProgram(static_cast<GLuint>(pipeline.Handle()));
    }

    void SetViewport(const Viewport& viewport)
    {
      FWOG_ASSERT(context->isRendering);

      SetViewportInternal(viewport, context->lastViewport, false);

      context->lastViewport = viewport;
    }

    void SetScissor(const Rect2D& scissor)
    {
      FWOG_ASSERT(context->isRendering);

      if (!context->scissorEnabled)
      {
        glEnable(GL_SCISSOR_TEST);
        context->scissorEnabled = true;
      }

      if (scissor == context->lastScissor)
      {
        return;
      }

      glScissor(scissor.offset.x, scissor.offset.y, scissor.extent.width, scissor.extent.height);

      context->lastScissor = scissor;
    }

    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride)
    {
      FWOG_ASSERT(context->isRendering);

      glVertexArrayVertexBuffer(context->currentVao,
                                bindingIndex,
                                buffer.Handle(),
                                static_cast<GLintptr>(offset),
                                static_cast<GLsizei>(stride));
    }

    void BindIndexBuffer(const Buffer& buffer, IndexType indexType)
    {
      FWOG_ASSERT(context->isRendering);

      context->isIndexBufferBound = true;
      context->currentIndexType = indexType;
      glVertexArrayElementBuffer(context->currentVao, buffer.Handle());
    }

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
      FWOG_ASSERT(context->isRendering);

      glDrawArraysInstancedBaseInstance(detail::PrimitiveTopologyToGL(context->currentTopology),
                                        firstVertex,
                                        vertexCount,
                                        instanceCount,
                                        firstInstance);
    }

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
      FWOG_ASSERT(context->isRendering);
      FWOG_ASSERT(context->isIndexBufferBound);

      // double cast is needed to prevent compiler from complaining about 32->64 bit pointer cast
      glDrawElementsInstancedBaseVertexBaseInstance(
        detail::PrimitiveTopologyToGL(context->currentTopology),
        indexCount,
        detail::IndexTypeToGL(context->currentIndexType),
        reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex * GetIndexSize(context->currentIndexType))),
        instanceCount,
        vertexOffset,
        firstInstance);
    }

    void DrawIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, uint32_t drawCount, uint32_t stride)
    {
      FWOG_ASSERT(context->isRendering);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glMultiDrawArraysIndirect(detail::PrimitiveTopologyToGL(context->currentTopology),
                                reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
                                drawCount,
                                stride);
    }

    void DrawIndirectCount(const Buffer& commandBuffer,
                           uint64_t commandBufferOffset,
                           const Buffer& countBuffer,
                           uint64_t countBufferOffset,
                           uint32_t maxDrawCount,
                           uint32_t stride)
    {
      FWOG_ASSERT(context->isRendering);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glBindBuffer(GL_PARAMETER_BUFFER, countBuffer.Handle());
      glMultiDrawArraysIndirectCount(detail::PrimitiveTopologyToGL(context->currentTopology),
                                     reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
                                     static_cast<GLintptr>(countBufferOffset),
                                     maxDrawCount,
                                     stride);
    }

    void DrawIndexedIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, uint32_t drawCount, uint32_t stride)
    {
      FWOG_ASSERT(context->isRendering);
      FWOG_ASSERT(context->isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glMultiDrawElementsIndirect(detail::PrimitiveTopologyToGL(context->currentTopology),
                                  detail::IndexTypeToGL(context->currentIndexType),
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
                                  drawCount,
                                  stride);
    }

    void DrawIndexedIndirectCount(const Buffer& commandBuffer,
                                  uint64_t commandBufferOffset,
                                  const Buffer& countBuffer,
                                  uint64_t countBufferOffset,
                                  uint32_t maxDrawCount,
                                  uint32_t stride)
    {
      FWOG_ASSERT(context->isRendering);
      FWOG_ASSERT(context->isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glBindBuffer(GL_PARAMETER_BUFFER, countBuffer.Handle());
      glMultiDrawElementsIndirectCount(detail::PrimitiveTopologyToGL(context->currentTopology),
                                       detail::IndexTypeToGL(context->currentIndexType),
                                       reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
                                       static_cast<GLintptr>(countBufferOffset),
                                       maxDrawCount,
                                       stride);
    }

    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      FWOG_ASSERT(context->isRendering || context->isComputeActive);

      if (size == WHOLE_BUFFER)
      {
        size = buffer.Size() - offset;
      }

      glBindBufferRange(GL_UNIFORM_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      FWOG_ASSERT(context->isRendering || context->isComputeActive);

      if (size == WHOLE_BUFFER)
      {
        size = buffer.Size() - offset;
      }

      glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindSampledImage(uint32_t index, const Texture& texture, const Sampler& sampler)
    {
      FWOG_ASSERT(context->isRendering || context->isComputeActive);

      glBindTextureUnit(index, const_cast<Texture&>(texture).Handle());
      glBindSampler(index, sampler.Handle());
    }

    void BindImage(uint32_t index, const Texture& texture, uint32_t level)
    {
      FWOG_ASSERT(context->isRendering || context->isComputeActive);
      FWOG_ASSERT(level < texture.GetCreateInfo().mipLevels);
      FWOG_ASSERT(IsValidImageFormat(texture.GetCreateInfo().format));

      glBindImageTexture(index,
                         const_cast<Texture&>(texture).Handle(),
                         level,
                         GL_TRUE,
                         0,
                         GL_READ_WRITE,
                         detail::FormatToGL(texture.GetCreateInfo().format));
    }

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
      FWOG_ASSERT(context->isComputeActive);

      glDispatchCompute(groupCountX, groupCountY, groupCountZ);
    }

    void Dispatch(Extent3D groupCount)
    {
      FWOG_ASSERT(context->isComputeActive);

      glDispatchCompute(groupCount.width, groupCount.height, groupCount.depth);
    }

    void DispatchInvocations(uint32_t invocationCountX, uint32_t invocationCountY, uint32_t invocationCountZ)
    {
      DispatchInvocations(Extent3D{invocationCountX, invocationCountY, invocationCountZ});
    }

    void DispatchInvocations(Extent3D invocationCount)
    {
      FWOG_ASSERT(context->isComputeActive);

      const auto workgroupSize = context->lastComputePipelineWorkgroupSize;
      const auto groupCount = (invocationCount + workgroupSize - 1) / workgroupSize;

      glDispatchCompute(groupCount.width, groupCount.height, groupCount.depth);
    }

    void DispatchIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset)
    {
      FWOG_ASSERT(context->isComputeActive);

      glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, commandBuffer.Handle());
      glDispatchComputeIndirect(static_cast<GLintptr>(commandBufferOffset));
    }
  } // namespace Cmd
} // namespace Fwog