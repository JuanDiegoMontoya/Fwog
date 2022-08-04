#include <Fwog/Common.h>
#include <Fwog/detail/PipelineManager.h>
#include <Fwog/detail/Hash.h>
#include <Fwog/Shader.h>
#include <Fwog/Exception.h>
#include <unordered_map>

namespace Fwog::detail
{
  namespace
  {
    std::unordered_map<GLuint, std::shared_ptr<const GraphicsPipelineInfoOwning>> gGraphicsPipelines;
    std::unordered_map<GLuint, std::shared_ptr<const ComputePipelineInfoOwning>> gComputePipelines;

    GraphicsPipelineInfoOwning MakePipelineInfoOwning(const GraphicsPipelineInfo& info)
    {
      return GraphicsPipelineInfoOwning
      {
        .name = std::string(info.name),
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
    return GraphicsPipeline{ program };
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
      return ComputePipeline{ it->first };
    }

    auto owning = ComputePipelineInfoOwning{ .name = std::string(info.name) };
    gComputePipelines.insert({ program, std::make_shared<const ComputePipelineInfoOwning>()});
    return ComputePipeline{ program };
  }


  std::shared_ptr<const ComputePipelineInfoOwning> GetComputePipelineInternal(ComputePipeline pipeline)
  {
    if (auto it = gComputePipelines.find(static_cast<GLuint>(pipeline.id)); it != gComputePipelines.end())
    {
      return it->second;
    }
    return nullptr;
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
