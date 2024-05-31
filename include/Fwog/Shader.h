#pragma once
#include <Fwog/Config.h>
#include <cstdint>
#include <string_view>
#include <span>

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

#if FWOG_VCC_ENABLE
  struct ShaderCppInfo
  {
    // Shady doesn't properly support non-main entry points for GLSL, so this is omitted for now
    //const char* entryPoint = "main";
    std::string_view source;
  };
#endif

  struct SpecializationConstant
  {
    uint32_t index;
    uint32_t value;
  };

  struct ShaderSpirvInfo
  {
    const char* entryPoint = "main";
    std::span<const uint32_t> code;
    std::span<const SpecializationConstant> specializationConstants;
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
#if FWOG_VCC_ENABLE
    /// @brief Constructs a shader from C++
    explicit Shader(PipelineStage stage, const ShaderCppInfo& cppInfo, std::string_view name = "");
#endif
    /// @brief Constructs a shader from SPIR-V
    explicit Shader(PipelineStage stage, const ShaderSpirvInfo& spirvInfo, std::string_view name = "");
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

  namespace detail
  {
    // Checks shader compile status and throws if it failed
    void ValidateShader(uint32_t id);
  }
} // namespace Fwog