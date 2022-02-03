#include <gsdf/Rendering.h>
#include <gsdf/detail/ApiToEnum.h>
#include <gsdf/Common.h>
#include <vector>

static void GLEnableOrDisable(GLenum state, GLboolean value)
{
  if (value)
    glEnable(state);
  else
    glDisable(state);
}

namespace GFX
{
  // rendering cannot be suspended/resumed, nor done on multiple threads
  // since only one rendering instance can be active at a time, we store some state here
  namespace
  {
    bool isRendering = false;
    bool isPipelineBound = false;

    PrimitiveTopology sTopology;
    GLuint sVao = 0;
    GLuint sFbo = 0;
  }

  void BeginRendering(const RenderInfo& renderInfo)
  {
    GSDF_ASSERT(!isRendering && "Cannot call BeginRendering when rendering");
    isRendering = true;

    const auto& ri = renderInfo;
    glDeleteFramebuffers(1, &sFbo);
    glCreateFramebuffers(1, &sFbo);
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < ri.colorAttachments.size(); i++)
    {
      const auto& attachment = ri.colorAttachments[i];
      glNamedFramebufferTexture(sFbo, GL_COLOR_ATTACHMENT0 + i, attachment.textureView->Handle(), 0);
      drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glNamedFramebufferDrawBuffers(sFbo, drawBuffers.size(), drawBuffers.data());
    for (size_t i = 0; i < ri.colorAttachments.size(); i++)
    {
      const auto& attachment = ri.colorAttachments[i];
      if (attachment.clearOnLoad)
      {
        // TODO: choose a glClearNamedFramebuffer* depending on the internal format of the texture
        auto format = attachment.textureView->CreateInfo().format;
        auto formatClass = detail::FormatToInternalType(format);
        switch (formatClass)
        {
        case GFX::detail::GlFormatClass::FLOAT:
          break;
        case GFX::detail::GlFormatClass::SINT:
          break;
        case GFX::detail::GlFormatClass::UINT:
          break;
        case GFX::detail::GlFormatClass::LONG:
          break;
        default:
          break;
        }
      }
    }
    if (ri.depthAttachment && ri.stencilAttachment && ri.depthAttachment->textureView == ri.stencilAttachment->textureView)
    {
      glNamedFramebufferTexture(sFbo, GL_DEPTH_STENCIL_ATTACHMENT, ri.depthAttachment->textureView->Handle(), 0);
    }
    else if (ri.depthAttachment)
    {
      glNamedFramebufferTexture(sFbo, GL_DEPTH_ATTACHMENT, ri.depthAttachment->textureView->Handle(), 0);
    }
    else if (ri.stencilAttachment)
    {
      glNamedFramebufferTexture(sFbo, GL_STENCIL_ATTACHMENT, ri.stencilAttachment->textureView->Handle(), 0);
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
  }

  void EndRendering()
  {
    GSDF_ASSERT(isRendering && "Cannot call EndRendering when not rendering");
    isPipelineBound = false;
    isRendering = false;
  }

  namespace Cmd
  {
    void BindPipeline(const GraphicsPipelineInfo& pipeline)
    {
      isPipelineBound = true;

      //////////////////////////////////////////////////////////////// input assembly
      const auto& ias = pipeline.inputAssemblyState;
      GLEnableOrDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX, ias.primitiveRestartEnable);
      sTopology = ias.topology;

      //////////////////////////////////////////////////////////////// vertex input
      const auto& vis = pipeline.vertexInputState;
      glDeleteVertexArrays(1, &sVao);
      glCreateVertexArrays(1, &sVao);
      for (uint32_t i = 0; i < vis.vertexBindingDescriptions.size(); i++)
      {
        const auto& desc = vis.vertexBindingDescriptions[i];
        glEnableVertexArrayAttrib(sVao, i);
        glVertexArrayAttribBinding(sVao, i, desc.binding);

        auto type = detail::FormatToTypeGL(desc.format);
        auto size = detail::FormatToSizeGL(desc.format);
        auto normalized = detail::IsFormatNormalizedGL(desc.format);
        auto internalType = detail::FormatToInternalType(desc.format);
        switch (internalType)
        {
        case detail::GlFormatClass::FLOAT:
          glVertexArrayAttribFormat(sVao, i, size, type, normalized, desc.offset);
          break;
        case detail::GlFormatClass::SINT: [[fallthrough]]
        case detail::GlFormatClass::UINT:
          glVertexArrayAttribIFormat(sVao, i, size, type, desc.offset);
          break;
        case detail::GlFormatClass::LONG:
          glVertexArrayAttribLFormat(sVao, i, size, type, desc.offset);
          break;
        default: GSDF_UNREACHABLE;
        }
      }
      glBindVertexArray(sVao);

      //////////////////////////////////////////////////////////////// rasterization
      const auto& rs = pipeline.rasterizationState;
      GLEnableOrDisable(GL_DEPTH_CLAMP, rs.depthClampEnable);
      glPolygonMode(GL_FRONT_AND_BACK, detail::PolygonModeToGL(rs.polygonMode));
      GLEnableOrDisable(GL_CULL_FACE, rs.cullMode != CullModeBits::NONE);
      if (rs.cullMode != CullModeBits::NONE)
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
      const auto& ds = pipeline.depthStencilState;
      GLEnableOrDisable(GL_DEPTH_TEST, ds.depthTestEnable);
      glDepthMask(ds.depthWriteEnable);
      // TODO: stencil state

      //////////////////////////////////////////////////////////////// color blending state
      const auto& cb = pipeline.colorBlendState;
      GLEnableOrDisable(GL_COLOR_LOGIC_OP, cb.logicOpEnable);
      if (cb.logicOpEnable)
      {
        glLogicOp(detail::LogicOpToGL(cb.logicOp));
      }
      glBlendColor(cb.blendConstants[0], cb.blendConstants[1], cb.blendConstants[2], cb.blendConstants[3]);
      for (size_t i = 0; i < cb.attachments.size(); i++)
      {
        const auto& cba = cb.attachments[i];
        glBlendFuncSeparatei(i, 
          detail::BlendFactorToGL(cba.srcColorBlendFactor),
          detail::BlendFactorToGL(cba.dstColorBlendFactor),
          detail::BlendFactorToGL(cba.srcAlphaBlendFactor),
          detail::BlendFactorToGL(cba.dstAlphaBlendFactor));
        glBlendEquationSeparatei(i, detail::BlendOpToGL(cba.colorBlendOp), detail::BlendOpToGL(cba.alphaBlendOp));
        glColorMaski(i, 
          (cba.colorWriteMask & ColorComponentFlag::R_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask & ColorComponentFlag::G_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask & ColorComponentFlag::B_BIT) != ColorComponentFlag::NONE,
          (cba.colorWriteMask & ColorComponentFlag::A_BIT) != ColorComponentFlag::NONE);
      }
    }

    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      GSDF_ASSERT(isRendering);
      glBindBufferRange(GL_UNIFORM_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size)
    {
      GSDF_ASSERT(isRendering);
      glBindBufferRange(GL_UNIFORM_BUFFER, index, buffer.Handle(), offset, size);
    }

    void BindSampledImage(uint32_t index, const TextureView& textureView, const TextureSampler& sampler)
    {
      GSDF_ASSERT(isRendering);
      glBindTextureUnit(index, textureView.Handle());
      glBindSampler(index, sampler.Handle());
    }

    void BindImage(uint32_t index, const TextureView& textureView, uint32_t level)
    {
      GSDF_ASSERT(isRendering);
      GSDF_ASSERT(level < textureView.CreateInfo().numLevels);
      glBindImageTexture(index, textureView.Handle(), level, GL_TRUE, 0, GL_READ_WRITE, detail::FormatToGL(textureView.CreateInfo().format));
    }
  }
}