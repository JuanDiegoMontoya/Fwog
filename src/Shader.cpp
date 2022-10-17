#include <Fwog/Common.h>
#include <Fwog/Exception.h>
#include <Fwog/Shader.h>
#include <string>
#include <string_view>
#include <utility>

namespace Fwog
{
  namespace
  {
    GLenum PipelineStageToGL(PipelineStage stage)
    {
      switch (stage)
      {
      case PipelineStage::VERTEX_SHADER: return GL_VERTEX_SHADER;
      case PipelineStage::FRAGMENT_SHADER: return GL_FRAGMENT_SHADER;
      case PipelineStage::COMPUTE_SHADER: return GL_COMPUTE_SHADER;
      default: FWOG_UNREACHABLE; return 0;
      }
    }
  } // namespace

  Shader::Shader(PipelineStage stage, std::string_view source)
  {
    const GLchar* strings = source.data();

    GLuint id = glCreateShader(PipelineStageToGL(stage));
    glShaderSource(id, 1, &strings, nullptr);
    glCompileShader(id);

    GLint success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      std::string infoLog;
      const GLsizei infoLength = 512;
      infoLog.resize(infoLength + 1, '\0');
      glGetShaderInfoLog(id, infoLength, nullptr, infoLog.data());
      throw ShaderCompilationException("Failed to compile shader source.\n" + infoLog);
    }

    id_ = id;
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
    glDeleteShader(id_);
  }
} // namespace Fwog