#include <Fwog/Timer.h>
#include <numeric>
#include FWOG_OPENGL_HEADER

namespace Fwog
{
  TimerQuery::TimerQuery()
  {
    glGenQueries(2, queries);
    glQueryCounter(queries[0], GL_TIMESTAMP);
  }

  TimerQuery::~TimerQuery()
  {
    glDeleteQueries(2, queries);
  }

  uint64_t TimerQuery::GetTimestamp()
  {
    int complete = 0;
    glQueryCounter(queries[1], GL_TIMESTAMP);
    while (!complete)
      glGetQueryObjectiv(queries[1], GL_QUERY_RESULT_AVAILABLE, &complete);
    uint64_t startTime, endTime;
    glGetQueryObjectui64v(queries[0], GL_QUERY_RESULT, &startTime);
    glGetQueryObjectui64v(queries[1], GL_QUERY_RESULT, &endTime);
    std::swap(queries[0], queries[1]);
    return endTime - startTime;
  }

  TimerQueryAsync::TimerQueryAsync(uint32_t N) : capacity_(N)
  {
    FWOG_ASSERT(capacity_ > 0);
    queries = new uint32_t[capacity_ * 2];
    glGenQueries(capacity_ * 2, queries);
  }

  TimerQueryAsync::~TimerQueryAsync()
  {
    glDeleteQueries(capacity_ * 2, queries);
    delete[] queries;
  }

  void TimerQueryAsync::BeginZone()
  {
    // begin a query if there is at least one inactive
    if (count_ < capacity_)
    {
      glQueryCounter(queries[start_], GL_TIMESTAMP);
    }
  }

  void TimerQueryAsync::EndZone()
  {
    // end a query if there is at least one inactive
    if (count_ < capacity_)
    {
      glQueryCounter(queries[start_ + capacity_], GL_TIMESTAMP);
      start_ = (start_ + 1) % capacity_; // wrap
      count_++;
    }
  }

  std::optional<uint64_t> TimerQueryAsync::PopTimestamp()
  {
    // return nothing if there is no active query
    if (count_ == 0)
    {
      return std::nullopt;
    }

    // get the index of the oldest query
    uint32_t index = (start_ + capacity_ - count_) % capacity_;

    // getting the start result is a sanity check
    GLint startResultAvailable{};
    GLint endResultAvailable{};
    glGetQueryObjectiv(queries[index], GL_QUERY_RESULT_AVAILABLE, &startResultAvailable);
    glGetQueryObjectiv(queries[index + capacity_], GL_QUERY_RESULT_AVAILABLE, &endResultAvailable);

    // the oldest query's result is not available, abandon ship!
    if (startResultAvailable == GL_FALSE || endResultAvailable == GL_FALSE)
    {
      return std::nullopt;
    }

    // pop oldest timing and retrieve result
    count_--;
    uint64_t startTimestamp{};
    uint64_t endTimestamp{};
    glGetQueryObjectui64v(queries[index], GL_QUERY_RESULT, &startTimestamp);
    glGetQueryObjectui64v(queries[index + capacity_], GL_QUERY_RESULT, &endTimestamp);
    return endTimestamp - startTimestamp;
  }
} // namespace Fwog