#pragma once
#include <cstdint>
#include <optional>

/*
  A blocking fence sync object used for CPU-GPU sync.
*/
namespace GFX
{
  class Fence
  {
  public:
    static std::optional<Fence> Create();
    ~Fence();

    Fence(Fence&& old) noexcept;
    Fence& operator=(Fence&& old) noexcept;

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    void Signal();

    // returns how long (in ns) we were blocked for
    uint64_t Wait();

  private:
    Fence();
    void* sync_{};
  };
}