#include <Fwog/Shader.h>
#include <Fwog/detail/ContextState.h>
#include <Fwog/Exception.h>
#include <Fwog/detail/ShaderGLSL.h>
#if FWOG_VCC_ENABLE
#include <Fwog/detail/ShaderCPP.h>
#endif
#include <Fwog/detail/ShaderSPIRV.h>

#include <string>
#include <utility>

#include FWOG_OPENGL_HEADER

namespace Fwog
{
  Shader::Shader(PipelineStage stage, std::string_view source, std::string_view name)
  {
    id_ = detail::CompileShaderGLSL(stage, source);

    detail::ValidateShader(id_);
    if (!name.empty())
    {
      glObjectLabel(GL_SHADER, id_, static_cast<GLsizei>(name.length()), name.data());
    }
    detail::InvokeVerboseMessageCallback("Created shader with handle ", id_);
  }

#if FWOG_VCC_ENABLE == 1
  Shader::Shader(PipelineStage stage, const ShaderCppInfo& cppInfo, std::string_view name)
  {
    const auto glsl = detail::CompileShaderCppToGlsl(cppInfo.source);
    id_ = detail::CompileShaderGLSL(stage, glsl);

    detail::ValidateShader(id_);
    if (!name.empty())
    {
      glObjectLabel(GL_SHADER, id_, static_cast<GLsizei>(name.length()), name.data());
    }
    detail::InvokeVerboseMessageCallback("Created shader with handle ", id_);
  }
#endif

  Shader::Shader(PipelineStage stage, const ShaderSpirvInfo& spirvInfo, std::string_view name)
  {
    id_ = detail::CompileShaderSpirv(stage, spirvInfo);

    detail::ValidateShader(id_);
    if (!name.empty())
    {
      glObjectLabel(GL_SHADER, id_, static_cast<GLsizei>(name.length()), name.data());
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

void Fwog::detail::ValidateShader(uint32_t id)
{
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
}
