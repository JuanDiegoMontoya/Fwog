#pragma once
#include <Fwog/Config.h>
#include <cstdint>

namespace Fwog
{
  /// @brief An object used for CPU-GPU synchronization
  class Fence
  {
  public:
    explicit Fence();
    Fence(Fence&& old) noexcept;
    Fence& operator=(Fence&& old) noexcept;
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;
    ~Fence();

    /// @brief Inserts a fence into the command stream
    void Signal();

    /// @brief Waits for the fence to be signaled and returns
    /// @return How long (in nanoseconds) the fence blocked
    /// @todo Add timeout parameter
    uint64_t Wait();

  private:
    void DeleteSync();

    void* sync_{};
  };
} // namespace Fwog