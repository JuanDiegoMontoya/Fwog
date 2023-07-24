#include <Fwog/Exception.h>
#include <Fwog/Shader.h>
#include <Fwog/detail/ContextState.h>

#include <string>
#include <string_view>
#include <utility>

#include FWOG_OPENGL_HEADER

namespace Fwog
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

  Shader::Shader(PipelineStage stage, std::string_view source)
  {
    const GLchar* strings = source.data();

    id_ = glCreateShader(PipelineStageToGL(stage));
    glShaderSource(id_, 1, &strings, nullptr);
    glCompileShader(id_);

    GLint success;
    glGetShaderiv(id_, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      GLint infoLength = 512;
      glGetShaderiv(id_, GL_INFO_LOG_LENGTH, &infoLength);
      auto infoLog = std::string(infoLength + 1, '\0');
      glGetShaderInfoLog(id_, infoLength, nullptr, infoLog.data());
      glDeleteShader(id_);
      throw ShaderCompilationException("Failed to compile shader source.\n" + infoLog);
    }

    detail::InvokeVerboseMessageCallback("Created shader with handle ", id_);
  }

  Shader::Shader(Shader&& old) noexcept : id_(std::exchange(old.id_, 0)) {}

  Shader& Shader::operator=(Shader&& old) noexcept
  {
    if (&old == this)
      return *this;
    this->~Shader();
    return *new (this) Shader(std::move(old));
  }

  Shader::~Shader()
  {
    detail::InvokeVerboseMessageCallback("Destroyed shader with handle ", id_);
    glDeleteShader(id_);
  }
} // namespace Fwog
