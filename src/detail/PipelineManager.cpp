#include <fwog/Common.h>
#include <fwog/detail/PipelineManager.h>
#include <fwog/detail/Hash.h>
#include <fwog/Shader.h>
#include <unordered_map>
#include <unordered_set>

namespace Fwog::detail
{
  namespace
  {
    std::unordered_map<GLuint, GraphicsPipelineInfoOwning> gGraphicsPipelines;
    std::unordered_set<GLuint> gComputePipelines;

    GraphicsPipelineInfoOwning MakePipelineInfoOwning(const GraphicsPipelineInfo& info)
    {
      return GraphicsPipelineInfoOwning
      {
        .inputAssemblyState = info.inputAssemblyState,
        .vertexInputState = { { info.vertexInputState.vertexBindingDescriptions.begin(), info.vertexInputState.vertexBindingDescriptions.end() } },
        .rasterizationState = info.rasterizationState,
        .depthState = info.depthState,
        .colorBlendState
        {
          .logicOpEnable = info.colorBlendState.logicOpEnable,
          .logicOp = info.colorBlendState.logicOp,
          .attachments = { info.colorBlendState.attachments.begin(), info.colorBlendState.attachments.end() },
          .blendConstants = { info.colorBlendState.blendConstants[0], info.colorBlendState.blendConstants[1], info.colorBlendState.blendConstants[2], info.colorBlendState.blendConstants[3] }
        }
      };
    }

    bool LinkProgram(GLuint program, std::string* outInfoLog)
    {
      glLinkProgram(program);

      GLint success{};
      glGetProgramiv(program, GL_LINK_STATUS, &success);
      if (!success)
      {
        if (outInfoLog)
        {
          const GLsizei length = 512;
          outInfoLog->resize(length + 1, '\0');
          glGetProgramInfoLog(program, length, nullptr, outInfoLog->data());
        }
        return false;
      }

      return true;
    }
  }

  std::optional<GraphicsPipeline> CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info)
  {
    FWOG_ASSERT(info.vertexShader && "A graphics pipeline must at least have a vertex shader");
    GLuint program = glCreateProgram();
    glAttachShader(program, info.vertexShader->Handle());
    if (info.fragmentShader)
    {
      glAttachShader(program, info.fragmentShader->Handle());
    }

    if (!LinkProgram(program, nullptr))
    {
      glDeleteProgram(program);
      return std::nullopt;
    }

    if (auto it = gGraphicsPipelines.find(program); it != gGraphicsPipelines.end())
    {
      return GraphicsPipeline{ it->first };
    }

    auto owning = MakePipelineInfoOwning(info);
    gGraphicsPipelines.insert({ program, owning });
    return GraphicsPipeline{ program };
  }

  const GraphicsPipelineInfoOwning* GetGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    if (auto it = gGraphicsPipelines.find(pipeline.id); it != gGraphicsPipelines.end())
    {
      return &it->second;
    }
    return nullptr;
  }

  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    auto it = gGraphicsPipelines.find(pipeline.id);
    if (it == gGraphicsPipelines.end())
    {
      return false;
    }

    glDeleteProgram(pipeline.id);
    gGraphicsPipelines.erase(it);
    return true;
  }

  std::optional<ComputePipeline> CompileComputePipelineInternal(const ComputePipelineInfo& info)
  {
    FWOG_ASSERT(info.shader);
    GLuint program = glCreateProgram();
    glAttachShader(program, info.shader->Handle());

    if (!LinkProgram(program, nullptr))
    {
      glDeleteProgram(program);
      return std::nullopt;
    }

    if (auto it = gComputePipelines.find(program); it != gComputePipelines.end())
    {
      return ComputePipeline{ *it };
    }

    gComputePipelines.insert({ program });
    return ComputePipeline{ program };
  }

  bool DestroyComputePipelineInternal(ComputePipeline pipeline)
  {
    auto it = gComputePipelines.find(pipeline.id);
    if (it == gComputePipelines.end())
    {
      return false;
    }

    glDeleteProgram(pipeline.id);
    gComputePipelines.erase(it);
    return true;
  }
}