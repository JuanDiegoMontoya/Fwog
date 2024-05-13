#include <Fwog/detail/ShaderGLSL.h>
#include <Fwog/detail/ContextState.h>
#include <Fwog/Exception.h>

#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  namespace
  {
    GLenum PipelineStageToGL(PipelineStage stage)
    {
      switch (stage)
      {
      case PipelineStage::VERTEX_SHADER: return GL_VERTEX_SHADER;
      case PipelineStage::TESSELLATION_CONTROL_SHADER: return GL_TESS_CONTROL_SHADER;
      case PipelineStage::TESSELLATION_EVALUATION_SHADER: return GL_TESS_EVALUATION_SHADER;
      case PipelineStage::FRAGMENT_SHADER: return GL_FRAGMENT_SHADER;
      case PipelineStage::COMPUTE_SHADER: return GL_COMPUTE_SHADER;
      default: FWOG_UNREACHABLE; return 0;
      }
    }
  } // namespace

  uint32_t CompileShaderGLSL(PipelineStage stage, std::string_view sourceGLSL, std::string_view name)
  {
    const GLchar* strings = sourceGLSL.data();

    uint32_t id = glCreateShader(PipelineStageToGL(stage));
    glShaderSource(id, 1, &strings, nullptr);
    glCompileShader(id);

    GLint success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      GLint infoLength = 512;
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLength);
      auto infoLog = std::string(infoLength + 1, '\0');
      glGetShaderInfoLog(id, infoLength, nullptr, infoLog.data());
      glDeleteShader(id);
      throw ShaderCompilationException("Failed to compile shader source.\n" + infoLog);
    }

    if (!name.empty())
    {
      glObjectLabel(GL_SHADER, id, static_cast<GLsizei>(name.length()), name.data());
    }

    detail::InvokeVerboseMessageCallback("Created shader with handle ", id);
    return id;
  }
} // namespace Fwog::detail