#include <fwog/Common.h>
#include <fwog/Shader.h>
#include <string_view>
#include <string>
#include <optional>
#include <utility>

namespace Fwog
{
  namespace
  {
    GLenum PipelineStageToGL(PipelineStage stage)
    {
      switch (stage)
      {
      case PipelineStage::VERTEX_SHADER:   return GL_VERTEX_SHADER;
      case PipelineStage::FRAGMENT_SHADER: return GL_FRAGMENT_SHADER;
      case PipelineStage::COMPUTE_SHADER:  return GL_COMPUTE_SHADER;
      default: FWOG_UNREACHABLE; return 0;
      }
    }
  }

  Shader::Shader()
  {
  }

  std::optional<Shader> Shader::Create(PipelineStage stage, std::string_view source, std::string* outInfoLog)
  {
    const GLchar* strings = source.data();

    GLuint id = glCreateShader(PipelineStageToGL(stage));
    glShaderSource(id, 1, &strings, nullptr);
    glCompileShader(id);

    GLint success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      if (outInfoLog)
      {
        const GLsizei infoLength = 512;
        outInfoLog->resize(infoLength + 1, '\0');
        glGetShaderInfoLog(id, infoLength, nullptr, outInfoLog->data());
      }
      return std::nullopt;
    }

    Shader shader{};
    shader.id_ = id;
    return shader;
  }

  Shader::Shader(Shader&& old) noexcept
  {
    id_ = std::exchange(old.id_, 0);
  }

  Shader& Shader::operator=(Shader&& old) noexcept
  {
    if (&old == this) return *this;
    this->~Shader();
    id_ = std::exchange(old.id_, 0);
    return *this;
  }

  Shader::~Shader()
  {
    glDeleteShader(id_);
  }
}