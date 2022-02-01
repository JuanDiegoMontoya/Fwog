#include <gsdf/Rendering.h>
#include <gsdf/detail/ApiToEnum.h>
#include <gsdf/Common.h>

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
  }

  void BeginRendering(const RenderInfo& renderInfo)
  {
    GSDF_ASSERT(!isRendering && "Cannot call BeginRendering when rendering");
    isRendering = true;
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
      //////////////////////////////////////////////////////////////// input assembly
      const auto& ias = pipeline.inputAssemblyState;
      GLEnableOrDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX, ias.primitiveRestartEnable);
      sTopology = ias.topology;

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