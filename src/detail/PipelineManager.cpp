#include <Fwog/Common.h>
#include <Fwog/detail/PipelineManager.h>
#include <Fwog/detail/Hash.h>
#include <Fwog/Shader.h>
#include <Fwog/Exception.h>
#include <Fwog/Rendering.h>
#include <Fwog/Buffer.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// whether to perform optimization of binding the pipeline and submitting a dummy draw/dispatch when compiling
#define DUMMY 1

namespace Fwog::detail
{
  namespace
  {
    std::unordered_map<GLuint, std::shared_ptr<const GraphicsPipelineInfoOwning>> gGraphicsPipelines;
    std::unordered_set<GLuint> gComputePipelines;

    GraphicsPipelineInfoOwning MakePipelineInfoOwning(const GraphicsPipelineInfo& info)
    {
      return GraphicsPipelineInfoOwning
      {
        .inputAssemblyState = info.inputAssemblyState,
        .vertexInputState = { { info.vertexInputState.vertexBindingDescriptions.begin(), info.vertexInputState.vertexBindingDescriptions.end() } },
        .rasterizationState = info.rasterizationState,
        .depthState = info.depthState,
        .stencilState = info.stencilState,
        .colorBlendState
        {
          .logicOpEnable = info.colorBlendState.logicOpEnable,
          .logicOp = info.colorBlendState.logicOp,
          .attachments = { info.colorBlendState.attachments.begin(), info.colorBlendState.attachments.end() },
          .blendConstants = { info.colorBlendState.blendConstants[0], info.colorBlendState.blendConstants[1], info.colorBlendState.blendConstants[2], info.colorBlendState.blendConstants[3] }
        }
      };
    }

    bool LinkProgram(GLuint program, std::string& outInfoLog)
    {
      glLinkProgram(program);

      GLint success{};
      glGetProgramiv(program, GL_LINK_STATUS, &success);
      if (!success)
      {
        const GLsizei length = 512;
        outInfoLog.resize(length + 1, '\0');
        glGetProgramInfoLog(program, length, nullptr, outInfoLog.data());
        return false;
      }

      return true;
    }
  }

  GraphicsPipeline CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info)
  {
    FWOG_ASSERT(info.vertexShader && "A graphics pipeline must at least have a vertex shader");
    GLuint program = glCreateProgram();
    glAttachShader(program, info.vertexShader->Handle());
    if (info.fragmentShader)
    {
      glAttachShader(program, info.fragmentShader->Handle());
    }

    std::string infolog;
    if (!LinkProgram(program, infolog))
    {
      glDeleteProgram(program);
      throw PipelineCompilationException("Failed to compile graphics pipeline.\n" + infolog);
    }

    if (auto it = gGraphicsPipelines.find(program); it != gGraphicsPipelines.end())
    {
      return GraphicsPipeline{ it->first };
    }

    auto owning = MakePipelineInfoOwning(info);
    gGraphicsPipelines.insert({ program, std::make_shared<const GraphicsPipelineInfoOwning>(std::move(owning)) });
    auto pipeline = GraphicsPipeline{ program };

#if DUMMY
    static auto dummyIndirectBuffer = Buffer(DrawIndirectCommand{});
    Viewport viewport{};
    if (info.renderInfo)
    {
      RenderInfo renderInfo = *info.renderInfo;
      std::vector<RenderAttachment> cs(info.renderInfo->colorAttachments.size(), RenderAttachment{});
      RenderAttachment d{};
      RenderAttachment s{};
      for (size_t i = 0; i < cs.size(); i++)
      {
        cs[i] = renderInfo.colorAttachments[i];
        cs[i].clearOnLoad = false;
      }
      renderInfo.colorAttachments = cs;
      if (renderInfo.depthAttachment)
      {
        d = *renderInfo.depthAttachment;
        d.clearOnLoad = false;
        renderInfo.depthAttachment = &d;
      }
      if (renderInfo.stencilAttachment)
      {
        s = *renderInfo.stencilAttachment;
        s.clearOnLoad = false;
        renderInfo.stencilAttachment = &s;
      }
      BeginRendering(*info.renderInfo);
    }
    else
    {
      BeginSwapchainRendering({ .viewport = &viewport });
    }
    Cmd::BindGraphicsPipeline(pipeline);
    for (const auto& vertexDesc : info.vertexInputState.vertexBindingDescriptions)
    {
      Cmd::BindVertexBuffer(vertexDesc.binding, dummyIndirectBuffer, 0, 0);
    }
    Cmd::DrawIndirect(dummyIndirectBuffer, 0, 1, 0);
    EndRendering();
#endif

    return pipeline;
  }

  std::shared_ptr<const GraphicsPipelineInfoOwning> GetGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    if (auto it = gGraphicsPipelines.find(static_cast<GLuint>(pipeline.id)); it != gGraphicsPipelines.end())
    {
      return it->second;
    }
    return nullptr;
  }

  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    auto it = gGraphicsPipelines.find(static_cast<GLuint>(pipeline.id));
    if (it == gGraphicsPipelines.end())
    {
      return false;
    }

    glDeleteProgram(static_cast<GLuint>(pipeline.id));
    gGraphicsPipelines.erase(it);
    return true;
  }

  ComputePipeline CompileComputePipelineInternal(const ComputePipelineInfo& info)
  {
    FWOG_ASSERT(info.shader);
    GLuint program = glCreateProgram();
    glAttachShader(program, info.shader->Handle());

    std::string infolog;
    if (!LinkProgram(program, infolog))
    {
      glDeleteProgram(program);
      throw PipelineCompilationException("Failed to compile compute pipeline.\n" + infolog);
    }

    if (auto it = gComputePipelines.find(program); it != gComputePipelines.end())
    {
      return ComputePipeline{ *it };
    }

    gComputePipelines.insert({ program });
    auto pipeline = ComputePipeline{ program };

#if DUMMY
    static auto dummyIndirectBuffer = Buffer(DispatchIndirectCommand{});
    BeginCompute();
    Cmd::BindComputePipeline(pipeline);
    Cmd::DispatchIndirect(dummyIndirectBuffer, 0);
    EndCompute();
#endif

    return pipeline;
  }

  bool DestroyComputePipelineInternal(ComputePipeline pipeline)
  {
    auto it = gComputePipelines.find(static_cast<GLuint>(pipeline.id));
    if (it == gComputePipelines.end())
    {
      return false;
    }

    glDeleteProgram(static_cast<GLuint>(pipeline.id));
    gComputePipelines.erase(it);
    return true;
  }
}
