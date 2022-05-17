#pragma once
#include <optional>

namespace Fwog
{
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
    uint64_t GetTimestamp();

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
    // always call EndZone after BeginZone
    // never call BeginZone or EndZone twice in a row
    void BeginZone();
    void EndZone();

    // returns oldest query's result, if available
    // otherwise, returns std::nullopt
    [[nodiscard]] std::optional<uint64_t> PopTimestamp();

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
    TimerScoped(T& zone) : zone_(zone)
    {
      zone_.BeginZone();
    }

    ~TimerScoped()
    {
      zone_.EndZone();
    }

  private:
    T& zone_;
  };
}