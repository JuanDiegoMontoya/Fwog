#include <Fwog/detail/ShaderSPIRV.h>
#include <Fwog/detail/ContextState.h>
#include <Fwog/detail/ApiToEnum.h>

#include <vector>

#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  uint32_t CompileShaderSpirv(PipelineStage stage, const ShaderSpirvInfo& spirvInfo)
  {
    uint32_t id = glCreateShader(PipelineStageToGL(stage));
    glShaderBinary(1, &id, GL_SHADER_BINARY_FORMAT_SPIR_V, (const GLuint*)spirvInfo.code.data(), (GLsizei)spirvInfo.code.size_bytes());

    // Unzip specialization constants into two streams to feed to OpenGL
    auto indices = std::vector<uint32_t>(spirvInfo.specializationConstants.size());
    auto values = std::vector<uint32_t>(spirvInfo.specializationConstants.size());
    for (size_t i = 0; i < spirvInfo.specializationConstants.size(); i++)
    {
      indices[i] = spirvInfo.specializationConstants[i].index;
      values[i] = spirvInfo.specializationConstants[i].value;
    }

    glSpecializeShader(id, spirvInfo.entryPoint, (GLuint)spirvInfo.specializationConstants.size(), indices.data(), values.data());
    return id;
  }
} // namespace Fwog::detail
