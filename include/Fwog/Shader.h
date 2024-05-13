#pragma once
#include <Fwog/Config.h>
#include <cstdint>
#include <string_view>

namespace Fwog
{
  enum class PipelineStage
  {
    VERTEX_SHADER,
    TESSELLATION_CONTROL_SHADER,
    TESSELLATION_EVALUATION_SHADER,
    FRAGMENT_SHADER,
    COMPUTE_SHADER,
  };

  enum class SourceLanguage
  {
    GLSL,
#if FWOG_VCC_ENABLE
    CPP, // C++
#endif
  };

  struct ShaderSourceInfo
  {
    /// @brief Specifies the language of the source string
    SourceLanguage language = SourceLanguage::GLSL;
    /// @brief A GLSL or C++ source string
    std::string_view source;
  };

  /// @brief A shader object to be used in one or more GraphicsPipeline or ComputePipeline objects
  class Shader
  {
  public:
    /// @brief Constructs a shader from GLSL
    /// @param stage A pipeline stage
    /// @param source A GLSL source string
    /// @param name An optional debug identifier
    /// @throws ShaderCompilationException if the shader is malformed
    explicit Shader(PipelineStage stage, std::string_view source, std::string_view name = "");

    /// @brief Constructs a shader from 
    /// @param stage A pipeline stage
    /// @param sourceInfo Information about the source
    /// @param name An optional debug identifier
    explicit Shader(PipelineStage stage, const ShaderSourceInfo& sourceInfo, std::string_view name = "");
    Shader(const Shader&) = delete;
    Shader(Shader&& old) noexcept;
    Shader& operator=(const Shader&) = delete;
    Shader& operator=(Shader&& old) noexcept;
    ~Shader();

    /// @brief Gets the handle of the underlying OpenGL shader object
    /// @return The shader
    [[nodiscard]] uint32_t Handle() const
    {
      return id_;
    }

  private:
    uint32_t id_{};
  };
} // namespace Fwog