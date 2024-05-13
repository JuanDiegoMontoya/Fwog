#include <Fwog/Shader.h>
#include <Fwog/detail/ContextState.h>
#include <Fwog/detail/ShaderGLSL.h>
#if FWOG_VCC_ENABLE
#include <Fwog/detail/ShaderCPP.h>
#endif

#include <string>
#include <utility>

#include FWOG_OPENGL_HEADER

namespace Fwog
{
  Shader::Shader(PipelineStage stage, std::string_view source, std::string_view name)
  {
    id_ = detail::CompileShaderGLSL(stage, source, name);
  }

  Shader::Shader(PipelineStage stage, const ShaderSourceInfo& sourceInfo, std::string_view name)
  {
    switch (sourceInfo.language)
    {
    case SourceLanguage::GLSL:
    {
      id_ = detail::CompileShaderGLSL(stage, sourceInfo.source, name);
      break;
    }
#if FWOG_VCC_ENABLE
    case SourceLanguage::CPP:
    {
      auto glslSource = detail::CompileShaderCppToGlsl(sourceInfo.source);
      id_ = detail::CompileShaderGLSL(stage, glslSource, name);
      break;
    }
#endif
    default: FWOG_UNREACHABLE;
    }
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
