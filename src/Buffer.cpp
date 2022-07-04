#include <Fwog/Common.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/Buffer.h>

namespace Fwog
{
  Buffer::Buffer(const void* data, size_t size, BufferFlags flags)
    : size_(std::max(size, 1ull))
  {
    GLbitfield glflags = detail::BufferFlagsToGL(flags);
    glCreateBuffers(1, &id_);
    glNamedBufferStorage(id_, size, data, glflags);
  }

  Buffer::Buffer(size_t size, BufferFlags flags)
    : Buffer(nullptr, size, flags)
  {
  }

  Buffer::Buffer(TriviallyCopyableByteSpan data, BufferFlags flags)
    : Buffer(data.data(), data.size_bytes(), flags)
  {
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

  void Buffer::SubData(TriviallyCopyableByteSpan data, size_t destOffsetBytes) const
  {
    SubData(data.data(), data.size_bytes(), destOffsetBytes);
  }

  void Buffer::SubData(const void* data, size_t size, size_t offset) const
  {
    FWOG_ASSERT(size + offset <= Size());
    glNamedBufferSubData(id_, static_cast<GLuint>(offset), static_cast<GLuint>(size), data);
  }

  void* Buffer::Map() const
  {
    FWOG_ASSERT(!IsMapped() && "Buffers cannot be mapped more than once at a time");
    isMapped_ = true;
    return glMapNamedBuffer(id_, GL_WRITE_ONLY);
  }

  void Buffer::Unmap() const
  {
    FWOG_ASSERT(IsMapped() && "Buffers that aren't mapped cannot be unmapped");
    isMapped_ = false;
    glUnmapNamedBuffer(id_);
  }

  void Buffer::ClearSubData(size_t offset, 
    size_t size, 
    Format internalFormat, 
    UploadFormat uploadFormat, 
    UploadType uploadType, 
    const void* data) const
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