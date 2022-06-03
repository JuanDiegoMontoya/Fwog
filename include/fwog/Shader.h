#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <optional>

namespace Fwog
{
  enum class PipelineStage
  {
    VERTEX_SHADER,
    FRAGMENT_SHADER,
    COMPUTE_SHADER
  };

  class Shader
  {
  public:
    [[nodiscard]] static std::optional<Shader> Create(PipelineStage stage, std::string_view source, std::string* outInfoLog = nullptr);

    [[nodiscard]] uint32_t Handle() const { return id_; }

    Shader(const Shader&) = delete;
    Shader(Shader&& old) noexcept;
    Shader& operator=(const Shader&) = delete;
    Shader& operator=(Shader&& old) noexcept;
    ~Shader();

  private:
    Shader();
    uint32_t id_{};
  };
}
