#include <Fwog/Common.h>
#include <Fwog/Exception.h>
#include <Fwog/Shader.h>
#include <Fwog/detail/Hash.h>
#include <Fwog/detail/PipelineManager.h>
#include <unordered_map>

namespace Fwog::detail
{
  namespace
  {
    std::unordered_map<GLuint, std::shared_ptr<const GraphicsPipelineInfoOwning>> gGraphicsPipelines;
    std::unordered_map<GLuint, std::shared_ptr<const ComputePipelineInfoOwning>> gComputePipelines;

    GraphicsPipelineInfoOwning MakePipelineInfoOwning(const GraphicsPipelineInfo& info)
    {
      return GraphicsPipelineInfoOwning{
        .name = std::string(info.name),
        .inputAssemblyState = info.inputAssemblyState,
        .vertexInputState = {{info.vertexInputState.vertexBindingDescriptions.begin(),
                              info.vertexInputState.vertexBindingDescriptions.end()}},
        .rasterizationState = info.rasterizationState,
        .depthState = info.depthState,
        .stencilState = info.stencilState,
        .colorBlendState{
          .logicOpEnable = info.colorBlendState.logicOpEnable,
          .logicOp = info.colorBlendState.logicOp,
          .attachments = {info.colorBlendState.attachments.begin(), info.colorBlendState.attachments.end()},
          .blendConstants =
            {
              info.colorBlendState.blendConstants[0],
              info.colorBlendState.blendConstants[1],
              info.colorBlendState.blendConstants[2],
              info.colorBlendState.blendConstants[3],
            },
        },
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
  } // namespace

  uint64_t CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info)
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
      return it->first;
    }

    auto owning = MakePipelineInfoOwning(info);
    gGraphicsPipelines.insert({program, std::make_shared<const GraphicsPipelineInfoOwning>(std::move(owning))});
    return program;
  }

  std::shared_ptr<const GraphicsPipelineInfoOwning> GetGraphicsPipelineInternal(uint64_t pipeline)
  {
    if (auto it = gGraphicsPipelines.find(static_cast<GLuint>(pipeline)); it != gGraphicsPipelines.end())
    {
      return it->second;
    }
    return nullptr;
  }

  void DestroyGraphicsPipelineInternal(uint64_t pipeline)
  {
    auto it = gGraphicsPipelines.find(static_cast<GLuint>(pipeline));
    if (it == gGraphicsPipelines.end())
    {
      // Tried to delete a nonexistent pipeline.
      FWOG_UNREACHABLE;
      return;
    }

    glDeleteProgram(static_cast<GLuint>(pipeline));
    gGraphicsPipelines.erase(it);
  }

  uint64_t CompileComputePipelineInternal(const ComputePipelineInfo& info)
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
      return it->first;
    }

    auto owning = ComputePipelineInfoOwning{.name = std::string(info.name)};
    gComputePipelines.insert({program, std::make_shared<const ComputePipelineInfoOwning>()});
    return program;
  }

  std::shared_ptr<const ComputePipelineInfoOwning> GetComputePipelineInternal(uint64_t pipeline)
  {
    if (auto it = gComputePipelines.find(static_cast<GLuint>(pipeline)); it != gComputePipelines.end())
    {
      return it->second;
    }
    return nullptr;
  }

  void DestroyComputePipelineInternal(uint64_t pipeline)
  {
    auto it = gComputePipelines.find(static_cast<GLuint>(pipeline));
    if (it == gComputePipelines.end())
    {
      // Tried to delete a nonexistent pipeline.
      FWOG_UNREACHABLE;
      return;
    }

    glDeleteProgram(static_cast<GLuint>(pipeline));
    gComputePipelines.erase(it);
  }
} // namespace Fwog::detail
