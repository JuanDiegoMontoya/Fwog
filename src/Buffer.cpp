#include <Fwog/Common.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/Buffer.h>
#include <utility>

namespace Fwog
{
  Buffer::Buffer(const void* data, 
                 size_t size, 
                 BufferStorageFlags storageFlags, 
                 BufferMapFlags mapFlags)
    : size_(std::max(size, static_cast<size_t>(1)))
  {
    GLbitfield glflags = detail::BufferStorageFlagsToGL(storageFlags);
    glflags |= detail::BufferMapFlagsToGL(mapFlags);
    glCreateBuffers(1, &id_);
    glNamedBufferStorage(id_, size_, data, glflags);
  }

  Buffer::Buffer(size_t size,
                 BufferStorageFlags storageFlags,
                 BufferMapFlags mapFlags)
    : Buffer(nullptr, size, storageFlags, mapFlags)
  {
  }

  Buffer::Buffer(TriviallyCopyableByteSpan data,
                 BufferStorageFlags storageFlags,
                 BufferMapFlags mapFlags)
    : Buffer(data.data(), data.size_bytes(), storageFlags, mapFlags)
  {
  }

  Buffer::Buffer(Buffer&& old) noexcept
    : size_(std::exchange(old.size_, 0)),
      id_(std::exchange(old.id_, 0)),
      isMapped_(std::exchange(old.isMapped_, false))
  {
  }

  Buffer& Buffer::operator=(Buffer&& old) noexcept
  {
    if (&old == this) return *this;
    this->~Buffer();
    return *new(this) Buffer(std::move(old));
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

  void* Buffer::Map(BufferMapFlags flags) const
  {
    FWOG_ASSERT(!IsMapped() && "Buffers cannot be mapped more than once at a time");
    isMapped_ = true;
    return glMapNamedBufferRange(id_, 0, Size(), detail::BufferMapFlagsToGL(flags));
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
    glClearNamedBufferSubData(id_,
                              detail::FormatToGL(internalFormat),
                              offset,
                              size,
                              detail::UploadFormatToGL(uploadFormat),
                              detail::UploadTypeToGL(uploadType),
                              data);
  }
}
