#include <Fwog/Common.h>
#include <Fwog/Rendering.h>
#include <Fwog/Texture.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/detail/PipelineManager.h>
#include <Fwog/detail/FramebufferCache.h>
#include <Fwog/detail/VertexArrayCache.h>
#include <vector>
#include <memory>
#include <array>

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

namespace Fwog
{
  // rendering cannot be suspended/resumed, nor done on multiple threads
  // since only one rendering instance can be active at a time, we store some state here
  namespace
  {
    constexpr int MAX_COLOR_ATTACHMENTS = 8;
    bool isComputeActive = false;
    bool isRendering = false;
    bool isIndexBufferBound = false;
    bool isRenderingToSwapchain = false;

    // TODO: way to reset this pointer in case the user wants to do their own OpenGL operations (invalidate the cache)
    std::shared_ptr<const detail::GraphicsPipelineInfoOwning> sLastGraphicsPipeline{}; // shared_ptr is needed as the user can delete pipelines at any time
    const RenderInfo* sLastRenderInfo{};

    // these can be set at the start of rendering, so they need to be tracked separately from the other pipeline state
    std::array<ColorComponentFlags, MAX_COLOR_ATTACHMENTS> sLastColorMask = { };
    bool sLastDepthMask = true;
    uint32_t sLastStencilMask[2] = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };
    bool sInitViewport = true;
    Viewport sLastViewport = {};
    Rect2D sLastScissor = {};
    bool sScissorEnabled = false;

    PrimitiveTopology sTopology{};
    IndexType sIndexType{};
    GLuint sVao = 0;
    GLuint sFbo = 0;

    detail::FramebufferCache sFboCache;
    detail::VertexArrayCache sVaoCache;
  }

  void BeginSwapchainRendering(const SwapchainRenderInfo& renderInfo)
  {
    FWOG_ASSERT(!isRendering && "Cannot call BeginRendering when rendering");
    FWOG_ASSERT(!isComputeActive && "Cannot nest compute and rendering");
    isRendering = true;
    isRenderingToSwapchain = true;
    sLastRenderInfo = nullptr;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const auto& ri = renderInfo;
    GLbitfield clearBuffers = 0;

    if (ri.clearColorOnLoad)
    {
      if (sLastColorMask[0] != ColorComponentFlag::RGBA_BITS)
      {
        glColorMaski(0, true, true, true, true);
        sLastColorMask[0] = ColorComponentFlag::RGBA_BITS;
      }
      glClearNamedFramebufferfv(0, GL_COLOR, 0, ri.clearColorValue.f);
    }
    if (ri.clearDepthOnLoad)
    {
      if (sLastDepthMask == false)
      {
        glDepthMask(true);
        sLastDepthMask = true;
      }
      glClearNamedFramebufferfv(0, GL_DEPTH, 0, &ri.clearDepthValue);
    }
    if (ri.clearStencilOnLoad)
    {
      if (sLastStencilMask[0] == false || sLastStencilMask[1] == false)
      {
        glStencilMask(true);
        sLastStencilMask[0] = true;
        sLastStencilMask[1] = true;
      }
      glClearNamedFramebufferiv(0, GL_STENCIL, 0, &ri.clearStencilValue);
    }
    if (sInitViewport || ri.viewport->drawRect != sLastViewport.drawRect)
    {
      glViewport(ri.viewport->drawRect.offset.x, ri.viewport->drawRect.offset.y,
        ri.viewport->drawRect.extent.width, ri.viewport->drawRect.extent.height);
    }
    if (sInitViewport || ri.viewport->minDepth != sLastViewport.minDepth || ri.viewport->maxDepth != sLastViewport.maxDepth)
    {
      glDepthRangef(ri.viewport->minDepth, ri.viewport->maxDepth);
    }

    sLastViewport = *renderInfo.viewport;
    sInitViewport = false;
  }

  void BeginRendering(const RenderInfo& renderInfo)
  {
    FWOG_ASSERT(!isRendering && "Cannot call BeginRendering when rendering");
    FWOG_ASSERT(!isComputeActive && "Cannot nest compute and rendering");
    isRendering = true;

    //if (sLastRenderInfo == &renderInfo)
    //{
    //  return;
    //}

    sLastRenderInfo = &renderInfo;

    const auto& ri = renderInfo;

    std::vector<const Texture*> colorAttachments;
    colorAttachments.reserve(ri.colorAttachments.size());
    for (const auto& attachment : ri.colorAttachments)
    {
      colorAttachments.push_back(attachment.texture);
    }
    detail::RenderAttachments attachments
    {
      .colorAttachments = colorAttachments,
      .depthAttachment = ri.depthAttachment ? ri.depthAttachment->texture : nullptr,
      .stencilAttachment = ri.stencilAttachment ? ri.stencilAttachment->texture : nullptr,
    };

    sFbo = sFboCache.CreateOrGetCachedFramebuffer(attachments);
    glBindFramebuffer(GL_FRAMEBUFFER, sFbo);

    for (GLint i = 0; i < static_cast<GLint>(ri.colorAttachments.size()); i++)
    {
      const auto& attachment = ri.colorAttachments[i];
      if (attachment.clearOnLoad)
      {
        if (sLastColorMask[i] != ColorComponentFlag::RGBA_BITS)
        {
          glColorMaski(i, true, true, true, true);
          sLastColorMask[i] = ColorComponentFlag::RGBA_BITS;
        }
        auto format = attachment.texture->CreateInfo().format;
        auto baseTypeClass = detail::FormatToBaseTypeClass(format);
        switch (baseTypeClass)
        {
        case detail::GlBaseTypeClass::FLOAT:
          glClearNamedFramebufferfv(sFbo, GL_COLOR, i, attachment.clearValue.color.f);
          break;
        case detail::GlBaseTypeClass::SINT:
          glClearNamedFramebufferiv(sFbo, GL_COLOR, i, attachment.clearValue.color.i);
          break;
        case detail::GlBaseTypeClass::UINT:
          glClearNamedFramebufferuiv(sFbo, GL_COLOR, i, attachment.clearValue.color.ui);
          break;
        default: FWOG_UNREACHABLE;
        }
      }
    }

    if (ri.depthAttachment && ri.depthAttachment->clearOnLoad && ri.stencilAttachment && ri.stencilAttachment->clearOnLoad)
    {
      // clear depth and stencil simultaneously
      if (sLastDepthMask == false)
      {
        glDepthMask(true);
        sLastDepthMask = true;
      }
      if (sLastStencilMask[0] == false || sLastStencilMask[1] == false)
      {
        glStencilMask(true);
        sLastStencilMask[0] = true;
        sLastStencilMask[1] = true;
      }
      glClearNamedFramebufferfi(sFbo, GL_DEPTH_STENCIL, 0,
        ri.depthAttachment->clearValue.depthStencil.depth,
        ri.depthAttachment->clearValue.depthStencil.stencil);
    }
    else if ((ri.depthAttachment && ri.depthAttachment->clearOnLoad) && (!ri.stencilAttachment || !ri.stencilAttachment->clearOnLoad))
    {
      // clear just depth
      if (sLastDepthMask == false)
      {
        glDepthMask(true);
        sLastDepthMask = true;
      }
      glClearNamedFramebufferfv(sFbo, GL_DEPTH, 0, &ri.depthAttachment->clearValue.depthStencil.depth);
    }
    else if ((ri.stencilAttachment && ri.stencilAttachment->clearOnLoad) && (!ri.depthAttachment || !ri.depthAttachment->clearOnLoad))
    {
      // clear just stencil
      if (sLastStencilMask[0] == false || sLastStencilMask[1] == false)
      {
        glStencilMask(true);
        sLastStencilMask[0] = true;
        sLastStencilMask[1] = true;
      }
      glClearNamedFramebufferiv(sFbo, GL_STENCIL, 0, &ri.stencilAttachment->clearValue.depthStencil.stencil);
    }

    if (sInitViewport || ri.viewport->drawRect != sLastViewport.drawRect)
    {
      glViewport(ri.viewport->drawRect.offset.x, ri.viewport->drawRect.offset.y,
        ri.viewport->drawRect.extent.width, ri.viewport->drawRect.extent.height);
    }
    if (sInitViewport || ri.viewport->minDepth != sLastViewport.minDepth || ri.viewport->maxDepth != sLastViewport.maxDepth)
    {
      glDepthRangef(ri.viewport->minDepth, ri.viewport->maxDepth);
    }

    sLastViewport = *renderInfo.viewport;
    sInitViewport = false;
  }

  void EndRendering()
  {
    FWOG_ASSERT(isRendering && "Cannot call EndRendering when not rendering");
    isRendering = false;
    isIndexBufferBound = false;
    isRenderingToSwapchain = false;

    if (sScissorEnabled)
    {
      glDisable(GL_SCISSOR_TEST);
      sScissorEnabled = false;
    }
  }

  void BeginCompute()
  {
    FWOG_ASSERT(!isComputeActive);
    FWOG_ASSERT(!isRendering && "Cannot nest compute and rendering");
    isComputeActive = true;
  }

  void EndCompute()
  {
    FWOG_ASSERT(isComputeActive);
    isComputeActive = false;
  }

  void BlitTexture(
    const Texture& source, 
    const Texture& target, 
    Offset3D sourceOffset, 
    Offset3D targetOffset, 
    Extent3D sourceExtent, 
    Extent3D targetExtent, 
    Filter filter,
    AspectMask aspect)
  {
    detail::RenderAttachments sourceAttachments;
    detail::RenderAttachments destAttachments;
    sourceAttachments.colorAttachments.push_back(&source);
    destAttachments.colorAttachments.push_back(&target);
    auto fboSource = sFboCache.CreateOrGetCachedFramebuffer(sourceAttachments);
    auto fboDest = sFboCache.CreateOrGetCachedFramebuffer(destAttachments);
    glBlitNamedFramebuffer(
      fboSource,
      fboDest,
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
    detail::RenderAttachments attachments;
    attachments.colorAttachments.push_back(&source);
    auto fbo = sFboCache.CreateOrGetCachedFramebuffer(attachments);
    glBlitNamedFramebuffer(
      fbo,
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

  void CopyTexture(
    const Texture& source,
    const Texture& target,
    uint32_t sourceLevel,
    uint32_t targetLevel,
    Offset3D sourceOffset,
    Offset3D targetOffset,
    Extent3D extent)
  {
    glCopyImageSubData(
      source.Handle(),
      GL_TEXTURE,
      sourceLevel,
      sourceOffset.x,
      sourceOffset.y,
      sourceOffset.z,
      target.Handle(),
      GL_TEXTURE,
      targetLevel,
      targetOffset.x,
      targetOffset.y,
      targetOffset.z,
      extent.width,
      extent.height,
      extent.depth);
  }

  namespace Cmd
  {
    void BindGraphicsPipeline(GraphicsPipeline pipeline)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(pipeline.id != 0);

      auto pipelineState = detail::GetGraphicsPipelineInternal(pipeline);
      FWOG_ASSERT(pipelineState);

      if (sLastGraphicsPipeline == pipelineState)
      {
        return;
      }

      //////////////////////////////////////////////////////////////// shader program
      glUseProgram(static_cast<GLuint>(pipeline.id));

      //////////////////////////////////////////////////////////////// input assembly
      const auto& ias = pipelineState->inputAssemblyState;
      if (!sLastGraphicsPipeline || ias.primitiveRestartEnable != sLastGraphicsPipeline->inputAssemblyState.primitiveRestartEnable)
      {
        GLEnableOrDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX, ias.primitiveRestartEnable);
      }
      sTopology = ias.topology;

      //////////////////////////////////////////////////////////////// vertex input
      if (auto nextVao = sVaoCache.CreateOrGetCachedVertexArray(pipelineState->vertexInputState); nextVao != sVao)
      {
        sVao = nextVao;
        glBindVertexArray(sVao);
      }

      //////////////////////////////////////////////////////////////// rasterization
      const auto& rs = pipelineState->rasterizationState;
      if (!sLastGraphicsPipeline || rs.depthClampEnable != sLastGraphicsPipeline->rasterizationState.depthClampEnable)
      {
        GLEnableOrDisable(GL_DEPTH_CLAMP, rs.depthClampEnable);
      }

      if (!sLastGraphicsPipeline || rs.polygonMode != sLastGraphicsPipeline->rasterizationState.polygonMode)
      {
        glPolygonMode(GL_FRONT_AND_BACK, detail::PolygonModeToGL(rs.polygonMode));
      }

      if (!sLastGraphicsPipeline || rs.cullMode != sLastGraphicsPipeline->rasterizationState.cullMode)
      {
        GLEnableOrDisable(GL_CULL_FACE, rs.cullMode != CullMode::NONE);
        if (rs.cullMode != CullMode::NONE)
        {
          glCullFace(detail::CullModeToGL(rs.cullMode));
        }
      }

      if (!sLastGraphicsPipeline || rs.frontFace != sLastGraphicsPipeline->rasterizationState.frontFace)
      {
        glFrontFace(detail::FrontFaceToGL(rs.frontFace));
      }

      if (!sLastGraphicsPipeline || rs.depthBiasEnable != sLastGraphicsPipeline->rasterizationState.depthBiasEnable)
      {
        GLEnableOrDisable(GL_POLYGON_OFFSET_FILL, rs.depthBiasEnable);
        GLEnableOrDisable(GL_POLYGON_OFFSET_LINE, rs.depthBiasEnable);
        GLEnableOrDisable(GL_POLYGON_OFFSET_POINT, rs.depthBiasEnable);
      }

      if (!sLastGraphicsPipeline
        || rs.depthBiasSlopeFactor != sLastGraphicsPipeline->rasterizationState.depthBiasSlopeFactor
        || rs.depthBiasConstantFactor != sLastGraphicsPipeline->rasterizationState.depthBiasConstantFactor)
      {
        glPolygonOffset(rs.depthBiasSlopeFactor, rs.depthBiasConstantFactor);
      }

      if (!sLastGraphicsPipeline || rs.lineWidth != sLastGraphicsPipeline->rasterizationState.lineWidth)
      {
        glLineWidth(rs.lineWidth);
      }

      if (!sLastGraphicsPipeline || rs.pointSize != sLastGraphicsPipeline->rasterizationState.pointSize)
      {
        glPointSize(rs.pointSize);
      }

      //////////////////////////////////////////////////////////////// depth + stencil
      const auto& ds = pipelineState->depthState;
      if (!sLastGraphicsPipeline || ds.depthTestEnable != sLastGraphicsPipeline->depthState.depthTestEnable)
      {
        GLEnableOrDisable(GL_DEPTH_TEST, ds.depthTestEnable);
      }

      if (ds.depthTestEnable)
      {
        if (!sLastGraphicsPipeline || ds.depthWriteEnable != sLastGraphicsPipeline->depthState.depthWriteEnable)
        {
          if (ds.depthWriteEnable != sLastDepthMask)
          {
            glDepthMask(ds.depthWriteEnable);
            sLastDepthMask = ds.depthWriteEnable;
          }
        }

        if (!sLastGraphicsPipeline || ds.depthCompareOp != sLastGraphicsPipeline->depthState.depthCompareOp)
        {
          glDepthFunc(detail::CompareOpToGL(ds.depthCompareOp));
        }
      }

      const auto& ss = pipelineState->stencilState;
      if (!sLastGraphicsPipeline || ss.stencilTestEnable != sLastGraphicsPipeline->stencilState.stencilTestEnable)
      {
        GLEnableOrDisable(GL_STENCIL_TEST, ss.stencilTestEnable);
      }

      if (ss.stencilTestEnable)
      {
        if (!sLastGraphicsPipeline || !sLastGraphicsPipeline->stencilState.stencilTestEnable || ss.front != sLastGraphicsPipeline->stencilState.front)
        {
          glStencilOpSeparate(GL_FRONT, detail::StencilOpToGL(ss.front.failOp), detail::StencilOpToGL(ss.front.depthFailOp), detail::StencilOpToGL(ss.front.passOp));
          glStencilFuncSeparate(GL_FRONT, detail::CompareOpToGL(ss.front.compareOp), ss.front.reference, ss.front.compareMask);
          if (sLastStencilMask[0] != ss.front.writeMask)
          {
            glStencilMaskSeparate(GL_FRONT, ss.front.writeMask);
            sLastStencilMask[0] = ss.front.writeMask;
          }
        }

        if (!sLastGraphicsPipeline || !sLastGraphicsPipeline->stencilState.stencilTestEnable || ss.back != sLastGraphicsPipeline->stencilState.back)
        {
          glStencilOpSeparate(GL_BACK, detail::StencilOpToGL(ss.back.failOp), detail::StencilOpToGL(ss.back.depthFailOp), detail::StencilOpToGL(ss.back.passOp));
          glStencilFuncSeparate(GL_BACK, detail::CompareOpToGL(ss.back.compareOp), ss.back.reference, ss.back.compareMask);
          if (sLastStencilMask[1] != ss.back.writeMask)
          {
            glStencilMaskSeparate(GL_BACK, ss.back.writeMask);
            sLastStencilMask[1] = ss.back.writeMask;
          }
        }
      }

      //////////////////////////////////////////////////////////////// color blending state
      const auto& cb = pipelineState->colorBlendState;
      if (!sLastGraphicsPipeline || cb.logicOpEnable != sLastGraphicsPipeline->colorBlendState.logicOpEnable)
      {
        GLEnableOrDisable(GL_COLOR_LOGIC_OP, cb.logicOpEnable);
        if (!sLastGraphicsPipeline || !sLastGraphicsPipeline->colorBlendState.logicOpEnable || (cb.logicOpEnable && cb.logicOp != sLastGraphicsPipeline->colorBlendState.logicOp))
        {
          glLogicOp(detail::LogicOpToGL(cb.logicOp));
        }
      }

      if (!sLastGraphicsPipeline || std::memcmp(cb.blendConstants, sLastGraphicsPipeline->colorBlendState.blendConstants, sizeof(cb.blendConstants)) != 0)
      {
        glBlendColor(cb.blendConstants[0], cb.blendConstants[1], cb.blendConstants[2], cb.blendConstants[3]);
      }

      //FWOG_ASSERT((cb.attachments.empty()
      //  || (isRenderingToSwapchain && !cb.attachments.empty()))
      //  || sLastRenderInfo->colorAttachments.size() >= cb.attachments.size()
      //  && "There must be at least a color blend attachment for each render target, or none");

      if (!sLastGraphicsPipeline || cb.attachments.empty() != sLastGraphicsPipeline->colorBlendState.attachments.empty())
      {
        GLEnableOrDisable(GL_BLEND, !cb.attachments.empty());
      }

      for (GLuint i = 0; i < static_cast<GLuint>(cb.attachments.size()); i++)
      {
        const auto& cba = cb.attachments[i];
        if (sLastGraphicsPipeline && i < sLastGraphicsPipeline->colorBlendState.attachments.size() && cba == sLastGraphicsPipeline->colorBlendState.attachments[i])
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

        if (sLastColorMask[i] != cba.colorWriteMask)
        {
          glColorMaski(i,
            (cba.colorWriteMask & ColorComponentFlag::R_BIT) != ColorComponentFlag::NONE,
            (cba.colorWriteMask & ColorComponentFlag::G_BIT) != ColorComponentFlag::NONE,
            (cba.colorWriteMask & ColorComponentFlag::B_BIT) != ColorComponentFlag::NONE,
            (cba.colorWriteMask & ColorComponentFlag::A_BIT) != ColorComponentFlag::NONE);
          sLastColorMask[i] = cba.colorWriteMask;
        }
      }

      sLastGraphicsPipeline = pipelineState;
    }

    void BindComputePipeline(ComputePipeline pipeline)
    {
      FWOG_ASSERT(isComputeActive);
      FWOG_ASSERT(pipeline.id != 0);

      glUseProgram(static_cast<GLuint>(pipeline.id));
    }

    void SetViewport(const Viewport& viewport)
    {
      FWOG_ASSERT(isRendering);

      if (viewport == sLastViewport)
      {
        return;
      }

      glViewport(
        viewport.drawRect.offset.x,
        viewport.drawRect.offset.y,
        viewport.drawRect.extent.width,
        viewport.drawRect.extent.height);
      glDepthRangef(viewport.minDepth, viewport.maxDepth);

      sLastViewport = viewport;
    }

    void SetScissor(const Rect2D& scissor)
    {
      FWOG_ASSERT(isRendering);

      if (!sScissorEnabled)
      {
        glEnable(GL_SCISSOR_TEST);
        sScissorEnabled = true;
      }

      if (scissor == sLastScissor)
      {
        return;
      }

      glScissor(
        scissor.offset.x, 
        scissor.offset.y, 
        scissor.extent.width, 
        scissor.extent.height);

      sLastScissor = scissor;
    }

    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride)
    {
      FWOG_ASSERT(isRendering);

      glVertexArrayVertexBuffer(
        sVao,
        bindingIndex,
        buffer.Handle(),
        static_cast<GLintptr>(offset),
        static_cast<GLsizei>(stride));
    }

    void BindIndexBuffer(const Buffer& buffer, IndexType indexType)
    {
      FWOG_ASSERT(isRendering);

      isIndexBufferBound = true;
      sIndexType = indexType;
      glVertexArrayElementBuffer(sVao, buffer.Handle());
    }

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
      FWOG_ASSERT(isRendering);

      glDrawArraysInstancedBaseInstance(
        detail::PrimitiveTopologyToGL(sTopology),
        firstVertex,
        vertexCount,
        instanceCount,
        firstInstance);
    }

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(isIndexBufferBound);

      glDrawElementsInstancedBaseVertexBaseInstance(
        detail::PrimitiveTopologyToGL(sTopology),
        indexCount,
        detail::IndexTypeToGL(sIndexType),
        reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex * GetIndexSize(sIndexType))), // double cast is needed to prevent compiler from complaining about 32->64 bit pointer cast
        instanceCount,
        vertexOffset,
        firstInstance);
    }

    void DrawIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, uint32_t drawCount, uint32_t stride)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glMultiDrawArraysIndirect(
        detail::PrimitiveTopologyToGL(sTopology),
        reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
        drawCount,
        stride);
    }

    void DrawIndirectCount(const Buffer& commandBuffer, uint64_t commandBufferOffset, const Buffer& countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glBindBuffer(GL_PARAMETER_BUFFER, countBuffer.Handle());
      glMultiDrawArraysIndirectCount(
        detail::PrimitiveTopologyToGL(sTopology),
        reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
        static_cast<GLintptr>(countBufferOffset),
        maxDrawCount,
        stride);
    }

    void DrawIndexedIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, uint32_t drawCount, uint32_t stride)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glMultiDrawElementsIndirect(
        detail::PrimitiveTopologyToGL(sTopology),
        detail::IndexTypeToGL(sIndexType),
        reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
        drawCount,
        stride);
    }

    void DrawIndexedIndirectCount(const Buffer& commandBuffer, uint64_t commandBufferOffset, const Buffer& countBuffer, uint64_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(isIndexBufferBound);

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer.Handle());
      glBindBuffer(GL_PARAMETER_BUFFER, countBuffer.Handle());
      glMultiDrawElementsIndirectCount(
        detail::PrimitiveTopologyToGL(sTopology),
        detail::IndexTypeToGL(sIndexType),
        reinterpret_cast<void*>(static_cast<uintptr_t>(commandBufferOffset)),
        static_cast<GLintptr>(countBufferOffset),
        maxDrawCount,
        stride);
    }

    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      FWOG_ASSERT(isRendering || isComputeActive);

      glBindBufferRange(GL_UNIFORM_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      FWOG_ASSERT(isRendering || isComputeActive);

      glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindSampledImage(uint32_t index, const Texture& texture, const Sampler& sampler)
    {
      FWOG_ASSERT(isRendering || isComputeActive);

      glBindTextureUnit(index, texture.Handle());
      glBindSampler(index, sampler.Handle());
    }

    void BindImage(uint32_t index, const Texture& texture, uint32_t level)
    {
      FWOG_ASSERT(isRendering || isComputeActive);
      FWOG_ASSERT(level < texture.CreateInfo().mipLevels);

      glBindImageTexture(index, texture.Handle(), level, GL_TRUE, 0, GL_READ_WRITE, detail::FormatToGL(texture.CreateInfo().format));
    }

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
      FWOG_ASSERT(isComputeActive);

      glDispatchCompute(groupCountX, groupCountY, groupCountZ);
    }

    void DispatchIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset)
    {
      FWOG_ASSERT(isComputeActive);

      glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, commandBuffer.Handle());
      glDispatchComputeIndirect(static_cast<GLintptr>(commandBufferOffset));
    }

    void MemoryBarrier(MemoryBarrierAccessBits accessBits)
    {
      FWOG_ASSERT(isRendering || isComputeActive);

      glMemoryBarrier(detail::BarrierBitsToGL(accessBits));
    }
  }
}