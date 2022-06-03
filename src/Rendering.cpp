#include <fwog/Common.h>
#include <fwog/Rendering.h>
#include <fwog/Texture.h>
#include <fwog/Buffer.h>
#include <fwog/Pipeline.h>
#include <fwog/detail/ApiToEnum.h>
#include <fwog/detail/PipelineManager.h>
#include <fwog/detail/FramebufferCache.h>
#include <fwog/detail/VertexArrayCache.h>
#include <vector>

// helper function
static void GLEnableOrDisable(GLenum state, GLboolean value)
{
  if (value)
    glEnable(state);
  else
    glDisable(state);
}

static void GLSetMaskStates()
{
  glColorMask(true, true, true, true);
  glDepthMask(true);
  glStencilMask(true);
}

namespace Fwog
{
  // rendering cannot be suspended/resumed, nor done on multiple threads
  // since only one rendering instance can be active at a time, we store some state here
  namespace
  {
    bool isComputeActive = false;
    bool isRendering = false;
    bool isPipelineBound = false;
    bool isIndexBufferBound = false;
    bool isRenderingToSwapchain = false;

    GraphicsPipeline sLastGraphicsPipeline{}; // TODO: way to reset this in case the user wants to do own OpenGL operations (basically invalidate cached state)
    const RenderInfo* sLastRenderInfo{};

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
      auto f = ri.clearColorValue.f;
      glClearColor(f[0], f[1], f[2], f[3]);
      clearBuffers |= GL_COLOR_BUFFER_BIT;
    }
    if (ri.clearDepthOnLoad)
    {
      glClearDepthf(ri.clearDepthValue);
      clearBuffers |= GL_DEPTH_BUFFER_BIT;
    }
    if (ri.clearStencilOnLoad)
    {
      glClearStencil(ri.clearStencilValue);
      clearBuffers |= GL_STENCIL_BUFFER_BIT;
    }
    if (clearBuffers != 0)
    {
      GLSetMaskStates();
      glClear(clearBuffers);
    }
    glViewport(ri.viewport->drawRect.offset.x, ri.viewport->drawRect.offset.y,
      ri.viewport->drawRect.extent.width, ri.viewport->drawRect.extent.height);
    glDepthRangef(ri.viewport->minDepth, ri.viewport->maxDepth);
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

    std::vector<const TextureView*> colorAttachments;
    colorAttachments.reserve(ri.colorAttachments.size());
    for (const auto& attachment : ri.colorAttachments)
    {
      colorAttachments.push_back(attachment.textureView);
    }
    detail::RenderAttachments attachments
    {
      .colorAttachments = colorAttachments,
      .depthAttachment = ri.depthAttachment ? ri.depthAttachment->textureView : nullptr,
      .stencilAttachment = ri.stencilAttachment ? ri.stencilAttachment->textureView : nullptr,
    };

    sFbo = sFboCache.CreateOrGetCachedFramebuffer(attachments);
    glBindFramebuffer(GL_FRAMEBUFFER, sFbo);

    GLSetMaskStates();

    for (GLint i = 0; i < static_cast<GLint>(ri.colorAttachments.size()); i++)
    {
      const auto& attachment = ri.colorAttachments[i];
      if (attachment.clearOnLoad)
      {
        auto format = attachment.textureView->CreateInfo().format;
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
      glClearNamedFramebufferfi(sFbo, GL_DEPTH_STENCIL, 0, 
        ri.depthAttachment->clearValue.depthStencil.depth,
        ri.depthAttachment->clearValue.depthStencil.stencil);
    }
    else if ((ri.depthAttachment && ri.depthAttachment->clearOnLoad) && (!ri.stencilAttachment || !ri.stencilAttachment->clearOnLoad))
    {
      // clear just depth
      glClearNamedFramebufferfv(sFbo, GL_DEPTH, 0, &ri.depthAttachment->clearValue.depthStencil.depth);
    }
    else if ((ri.stencilAttachment && ri.stencilAttachment->clearOnLoad) && (!ri.depthAttachment || !ri.depthAttachment->clearOnLoad))
    {
      // clear just stencil
      glClearNamedFramebufferiv(sFbo, GL_STENCIL, 0, &ri.stencilAttachment->clearValue.depthStencil.stencil);
    }
    glViewport(ri.viewport->drawRect.offset.x, ri.viewport->drawRect.offset.y,
      ri.viewport->drawRect.extent.width, ri.viewport->drawRect.extent.height);
    glDepthRangef(ri.viewport->minDepth, ri.viewport->maxDepth);
  }

  void EndRendering()
  {
    FWOG_ASSERT(isRendering && "Cannot call EndRendering when not rendering");
    isPipelineBound = false;
    isRendering = false;
    isIndexBufferBound = false;
    isRenderingToSwapchain = false;
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

  namespace Cmd
  {
    void BindGraphicsPipeline(GraphicsPipeline pipeline)
    {
      FWOG_ASSERT(isRendering);
      FWOG_ASSERT(pipeline.id != 0);
      isPipelineBound = true;

      auto pipelineState = detail::GetGraphicsPipelineInternal(pipeline);
      FWOG_ASSERT(pipelineState);

      if (sLastGraphicsPipeline == pipeline)
      {
        return;
      }

      sLastGraphicsPipeline = pipeline;

      //////////////////////////////////////////////////////////////// shader program
      glUseProgram(static_cast<GLuint>(pipeline.id));

      //////////////////////////////////////////////////////////////// input assembly
      const auto& ias = pipelineState->inputAssemblyState;
      GLEnableOrDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX, ias.primitiveRestartEnable);
      sTopology = ias.topology;
      
      //////////////////////////////////////////////////////////////// vertex input
      sVao = sVaoCache.CreateOrGetCachedVertexArray(pipelineState->vertexInputState);
      glBindVertexArray(sVao);

      //////////////////////////////////////////////////////////////// rasterization
      const auto& rs = pipelineState->rasterizationState;
      GLEnableOrDisable(GL_DEPTH_CLAMP, rs.depthClampEnable);
      glPolygonMode(GL_FRONT_AND_BACK, detail::PolygonModeToGL(rs.polygonMode));
      GLEnableOrDisable(GL_CULL_FACE, rs.cullMode != CullMode::NONE);
      if (rs.cullMode != CullMode::NONE)
      {
        glCullFace(detail::CullModeToGL(rs.cullMode));
      }
      glFrontFace(detail::FrontFaceToGL(rs.frontFace));
      GLEnableOrDisable(GL_POLYGON_OFFSET_FILL, rs.depthBiasEnable);
      GLEnableOrDisable(GL_POLYGON_OFFSET_LINE, rs.depthBiasEnable);
      GLEnableOrDisable(GL_POLYGON_OFFSET_POINT, rs.depthBiasEnable);
      if (rs.depthBiasEnable)
      {
        glPolygonOffset(rs.depthBiasSlopeFactor, rs.depthBiasConstantFactor);
      }
      glLineWidth(rs.lineWidth);
      glPointSize(rs.pointSize);

      //////////////////////////////////////////////////////////////// depth + stencil
      const auto& ds = pipelineState->depthState;
      GLEnableOrDisable(GL_DEPTH_TEST, ds.depthTestEnable);
      if (ds.depthTestEnable)
      {
        glDepthMask(ds.depthWriteEnable);
        glDepthFunc(detail::CompareOpToGL(ds.depthCompareOp));
      }

      const auto& ss = pipelineState->stencilState;
      GLEnableOrDisable(GL_STENCIL_TEST, ss.stencilTestEnable);
      if (ss.stencilTestEnable)
      {
        glStencilOpSeparate(GL_FRONT, detail::StencilOpToGL(ss.front.failOp), detail::StencilOpToGL(ss.front.depthFailOp), detail::StencilOpToGL(ss.front.passOp));
        glStencilOpSeparate(GL_BACK, detail::StencilOpToGL(ss.back.failOp), detail::StencilOpToGL(ss.back.depthFailOp), detail::StencilOpToGL(ss.back.passOp));
        glStencilFuncSeparate(GL_FRONT, detail::CompareOpToGL(ss.front.compareOp), ss.front.reference, ss.front.compareMask);
        glStencilFuncSeparate(GL_BACK, detail::CompareOpToGL(ss.back.compareOp), ss.back.reference, ss.back.compareMask);
        glStencilMaskSeparate(GL_FRONT, ss.front.writeMask);
        glStencilMaskSeparate(GL_BACK, ss.back.writeMask);
      }

      //////////////////////////////////////////////////////////////// color blending state
      const auto& cb = pipelineState->colorBlendState;
      GLEnableOrDisable(GL_COLOR_LOGIC_OP, cb.logicOpEnable);
      if (cb.logicOpEnable)
      {
        glLogicOp(detail::LogicOpToGL(cb.logicOp));
      }
      glBlendColor(cb.blendConstants[0], cb.blendConstants[1], cb.blendConstants[2], cb.blendConstants[3]);
      FWOG_ASSERT((cb.attachments.empty()
        || (isRenderingToSwapchain && !cb.attachments.empty()))
        || sLastRenderInfo->colorAttachments.size() > cb.attachments.size()
        && "There must be at least a color blend attachment for each render target, or none");
      
      if (cb.attachments.empty())
      {
        glDisable(GL_BLEND);
      }
      
      for (GLuint i = 0; i < static_cast<GLuint>(cb.attachments.size()); i++)
      {
        const auto& cba = cb.attachments[i];
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

        glColorMaski(i,
          (cba.colorWriteMask& ColorComponentFlag::R_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask& ColorComponentFlag::G_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask& ColorComponentFlag::B_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask& ColorComponentFlag::A_BIT) != ColorComponentFlag::NONE);
      }
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
      glViewport(
        viewport.drawRect.offset.x,
        viewport.drawRect.offset.y,
        viewport.drawRect.extent.width,
        viewport.drawRect.extent.height);
      glDepthRangef(viewport.minDepth, viewport.maxDepth);
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
      FWOG_ASSERT(isRendering && isIndexBufferBound);
      glDrawElementsInstancedBaseVertexBaseInstance(
        detail::PrimitiveTopologyToGL(sTopology),
        indexCount,
        detail::IndexTypeToGL(sIndexType),
        reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex)), // double cast is needed to prevent compiler from complaining about 32->64 bit pointer cast
        instanceCount,
        vertexOffset,
        firstInstance);
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

    void BindSampledImage(uint32_t index, const TextureView& textureView, const TextureSampler& sampler)
    {
      FWOG_ASSERT(isRendering || isComputeActive);
      glBindTextureUnit(index, textureView.Handle());
      glBindSampler(index, sampler.Handle());
    }

    void BindImage(uint32_t index, const TextureView& textureView, uint32_t level)
    {
      FWOG_ASSERT(isRendering || isComputeActive);
      FWOG_ASSERT(level < textureView.CreateInfo().numLevels);
      glBindImageTexture(index, textureView.Handle(), level, GL_TRUE, 0, GL_READ_WRITE, detail::FormatToGL(textureView.CreateInfo().format));
    }

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
      FWOG_ASSERT(isComputeActive);
      glDispatchCompute(groupCountX, groupCountY, groupCountZ);
    }

    void MemoryBarrier(MemoryBarrierAccessBits accessBits)
    {
      FWOG_ASSERT(isRendering || isComputeActive);
      glMemoryBarrier(detail::BarrierBitsToGL(accessBits));
    }
  }
}