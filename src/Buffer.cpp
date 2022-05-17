#include <fwog/Common.h>
#include <fwog/detail/ApiToEnum.h>
#include <fwog/Buffer.h>

namespace Fwog
{
  std::optional<Buffer> Buffer::CreateInternal(const void* data, size_t size, BufferFlags flags)
  {
    size = std::max(size, 1ull);
    GLbitfield glflags = detail::BufferFlagsToGL(flags);
    Buffer buffer{};
    buffer.size_ = size;
    glCreateBuffers(1, &buffer.id_);
    glNamedBufferStorage(buffer.id_, size, data, glflags);
    return buffer;
  }

  Buffer::Buffer(Buffer&& old) noexcept
  {
    *this = std::move(old);
  }

  Buffer& Buffer::operator=(Buffer&& old) noexcept
  {
    if (&old == this) return *this;
    this->~Buffer();
    id_ = std::exchange(old.id_, 0);
    size_ = std::exchange(old.size_, 0);
    isMapped_ = std::exchange(old.isMapped_, false);
    return *this;
  }

  Buffer::~Buffer()
  {
    FWOG_ASSERT(!IsMapped() && "Buffers must not be mapped at time of destruction");
    if (id_)
    {
      glDeleteBuffers(1, &id_);
    }
  }

  void Buffer::SubData(const void* data, size_t size, size_t offset)
  {
    glNamedBufferSubData(id_, static_cast<GLuint>(offset), static_cast<GLuint>(size), data);
  }

  void* Buffer::GetMappedPointer()
  {
    FWOG_ASSERT(!IsMapped() && "Buffers cannot be mapped more than once at a time");
    isMapped_ = true;
    return glMapNamedBuffer(id_, GL_READ_WRITE);
  }

  void Buffer::UnmapPointer()
  {
    FWOG_ASSERT(IsMapped() && "Buffers that aren't mapped cannot be unmapped");
    isMapped_ = false;
    glUnmapNamedBuffer(id_);
  }

  void Buffer::ClearSubData(size_t offset, size_t size, Format internalFormat, UploadFormat uploadFormat, UploadType uploadType, const void* data)
  {
    glClearNamedBufferSubData(
      id_,
      detail::FormatToGL(internalFormat),
      offset,
      size,
      detail::UploadFormatToGL(uploadFormat),
      detail::UploadTypeToGL(uploadType),
      data);
  }
}