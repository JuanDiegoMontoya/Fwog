#pragma once
#include <Fwog/Config.h>
#include <cstdint>

/*
  A blocking fence sync object used for CPU-GPU sync.
*/
namespace Fwog
{
  class Fence
  {
  public:
    explicit Fence();
    Fence(Fence&& old) noexcept;
    Fence& operator=(Fence&& old) noexcept;
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;
    ~Fence();

    void Signal();

    // returns how long (in ns) we were blocked for
    // TODO: add timeout
    uint64_t Wait();

  private:
    void* sync_{};
  };
} // namespace Fwog