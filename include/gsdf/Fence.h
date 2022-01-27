#pragma once
#include <cstdint>
#include <optional>

/*
  A blocking fence sync object. Used for CPU-GPU sync.

  Usage:
  Create the Fence object *after* commands that rely on some protected memory
  (e.g. commands that cause the GPU to read or write to memory).

  Next, call Sync() on the previously made Fence immediately *before* modifying 
  protected memory. All commands issued before the fence object was created must 
  complete before Sync() will return.

  Notes:
  The timeout is infinite(!), so ensure you are properly double/triple buffering
  if you do not want Sync() to block. In those cases, treat this class as a 
  sanity check.

  Use this class primarily for ensuring persistently mapped buffers do not write 
  while the GPU is using them.
*/
typedef struct __GLsync* GLsync;
namespace GFX
{
  class Fence
  {
  public:
    Fence();
    ~Fence();

    Fence(const Fence&) = delete;
    Fence(Fence&&) = delete;
    Fence& operator=(const Fence&) = delete;
    Fence& operator=(Fence&&) = delete;

    // returns how long (in ns) we were blocked for
    uint64_t Sync();

  private:
    GLsync sync_;
  };

  // Synchronous single-buffered GPU-timeline timer.
  // Will induce massive pipeline stalls when using.
  // Useful for measuring the time a single pass or draw takes.
  // Use sparingly, and only if detailed perf data is needed for a particular draw.
  class TimerQuery
  {
  public:
    TimerQuery();
    ~TimerQuery();

    TimerQuery(const TimerQuery&) = delete;
    TimerQuery(TimerQuery&&) = delete;
    TimerQuery& operator=(const TimerQuery&) = delete;
    TimerQuery& operator=(TimerQuery&&) = delete;

    // returns how (in ns) we blocked for (BLOCKS)
    uint64_t Elapsed_ns();

  private:
    uint32_t queries[2];
  };

  // Async N-buffered timer query.
  // Does not induce pipeline stalls.
  // Useful for measuring performance of passes every frame without causing stalls.
  // However, the results returned may be from multiple frames ago,
  // and results are not guaranteed to be available.
  // In practice, setting N to 5 should allow at least one query to be available.
	class TimerQueryAsync
	{
  public:
    TimerQueryAsync(uint32_t N);
    ~TimerQueryAsync();

    TimerQueryAsync(const TimerQueryAsync&) = delete;
    TimerQueryAsync(TimerQueryAsync&&) = delete;
    TimerQueryAsync& operator=(const TimerQueryAsync&) = delete;
    TimerQueryAsync& operator=(TimerQueryAsync&&) = delete;

    // begins or ends a query
    // always call End after Begin
    // never call Begin or End twice in a row
    void Begin();
		void End();

    // returns oldest query's result, if available
    // otherwise, returns std::nullopt
		[[nodiscard]] std::optional<uint64_t> Elapsed_ns();

  private:
    uint32_t start_{}; // next timer to be used for measurement
    uint32_t count_{}; // number of timers 'buffered', ie measurement was started by result not read yet
    const uint32_t capacity_{};
    uint32_t* queries{};
	};

  // wraps any timer query to allow for easy scoping
  template<typename T>
	class TimerScoped
	{
  public:
    TimerScoped(T& zone)
      : zone_(zone)
		{
			zone_.Begin();
		}

		~TimerScoped()
		{
			zone_.End();
		}

  private:
    T& zone_;
	};
}