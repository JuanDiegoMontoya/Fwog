#pragma once
#include <cstdint>
#include <string_view>

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
    explicit Shader(PipelineStage stage, std::string_view source);
    Shader(const Shader&) = delete;
    Shader(Shader&& old) noexcept;
    Shader& operator=(const Shader&) = delete;
    Shader& operator=(Shader&& old) noexcept;
    ~Shader();

    [[nodiscard]] uint32_t Handle() const
    {
      return id_;
    }

  private:
    uint32_t id_{};
  };
} // namespace Fwog