#include <Fwog/detail/ShaderGLSL.h>
#include <Fwog/detail/ApiToEnum.h>

#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  uint32_t CompileShaderGLSL(PipelineStage stage, std::string_view sourceGLSL)
  {
    const GLchar* strings = sourceGLSL.data();

    uint32_t id = glCreateShader(PipelineStageToGL(stage));
    glShaderSource(id, 1, &strings, nullptr);
    glCompileShader(id);
    return id;
  }
} // namespace Fwog::detail