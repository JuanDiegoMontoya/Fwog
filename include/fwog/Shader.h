#pragma once
#include <cstdint>

namespace std
{
  template<class Elem>
  struct char_traits;
  template<class Elem, class Traits>
  class basic_string_view;
  using string_view = std::basic_string_view<char, std::char_traits<char>>;

  template<class T>
  class optional;

  template<class T>
  class allocator;
  template <class Elem, class Traits, class Alloc>
  class basic_string;
  using string = std::basic_string<char, std::char_traits<char>, std::allocator<char>>;
}

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
