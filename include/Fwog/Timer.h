#pragma once
#include <Fwog/Config.h>
#include <cstdint>
#include <optional>

namespace Fwog
{
  /// @brief Synchronous single-buffered GPU-timeline timer. Querying the timer will result in a stall
  /// as commands are flushed and waited on to complete
  /// 
  /// Use sparingly, and only if detailed perf data is needed for a particular draw.
  /// 
  /// @todo This class is in desparate need of an update
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

  /// @brief Async N-buffered timer query that does not induce pipeline stalls
  ///
  /// Useful for measuring performance of passes every frame without causing stalls.
  /// However, the results returned may be from multiple frames ago,
  /// and results are not guaranteed to be available.
  /// In practice, setting N to 5 should allow at least one query to be available every frame.
  class TimerQueryAsync
  {
  public:
    TimerQueryAsync(uint32_t N);
    ~TimerQueryAsync();

    TimerQueryAsync(const TimerQueryAsync&) = delete;
    TimerQueryAsync(TimerQueryAsync&&) = delete;
    TimerQueryAsync& operator=(const TimerQueryAsync&) = delete;
    TimerQueryAsync& operator=(TimerQueryAsync&&) = delete;

    /// @brief Begins a query zone
    ///
    /// @note EndZone must be called before another zone can begin
    void BeginZone();

    /// @brief Ends a query zone
    ///
    /// @note BeginZone must be called before a zone can end
    void EndZone();

    /// @brief Gets the latest available query
    /// @return The latest query, if available. Otherwise, std::nullopt is returned
    [[nodiscard]] std::optional<uint64_t> PopTimestamp();

  private:
    uint32_t start_{}; // next timer to be used for measurement
    uint32_t count_{}; // number of timers 'buffered', ie measurement was started by result not read yet
    const uint32_t capacity_{};
    uint32_t* queries{};
  };

  /// @brief RAII wrapper for TimerQueryAsync
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
} // namespace Fwog