#include <Fwog/Fence.h>
#include <numeric>
#include <utility>
#include <new>
#include FWOG_OPENGL_HEADER

namespace Fwog
{
  Fence::Fence() {}

  Fence::~Fence()
  {
    DeleteSync();
  }

  Fence::Fence(Fence&& old) noexcept : sync_(std::exchange(old.sync_, nullptr)) {}

  Fence& Fence::operator=(Fence&& old) noexcept
  {
    if (this == &old)
      return *this;
    this->~Fence();
    return *new (this) Fence(std::move(old));
  }

  void Fence::Signal()
  {
    FWOG_ASSERT(sync_ == nullptr);
    sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }

  uint64_t Fence::Wait()
  {
    FWOG_ASSERT(sync_ != nullptr);
    GLuint id;
    glGenQueries(1, &id);
    glBeginQuery(GL_TIME_ELAPSED, id);
    GLenum result = glClientWaitSync(reinterpret_cast<GLsync>(sync_),
                                     GL_SYNC_FLUSH_COMMANDS_BIT,
                                     std::numeric_limits<GLuint64>::max());
    FWOG_ASSERT(result == GL_CONDITION_SATISFIED);
    glEndQuery(GL_TIME_ELAPSED);
    uint64_t elapsed;
    glGetQueryObjectui64v(id, GL_QUERY_RESULT, &elapsed);
    glDeleteQueries(1, &id);
    DeleteSync();
    return elapsed;
  }

  void Fence::DeleteSync()
  {
    glDeleteSync(reinterpret_cast<GLsync>(sync_));
    sync_ = nullptr;
  }
} // namespace Fwog