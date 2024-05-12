#include <Fwog/Buffer.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/detail/ContextState.h>
#include <utility>
#include FWOG_OPENGL_HEADER

namespace Fwog
{
  constexpr int32_t RoundUp(int32_t numberToRoundUp, int32_t multipleOf)
  {
    assert(multipleOf);
    return ((numberToRoundUp + multipleOf - 1) / multipleOf) * multipleOf;
  }

  Buffer::Buffer(const void* data, size_t size, BufferStorageFlags storageFlags, std::string_view name)
    : size_(RoundUp(size, 16)), storageFlags_(storageFlags)
  {
    GLbitfield glflags = detail::BufferStorageFlagsToGL(storageFlags);
    glCreateBuffers(1, &id_);
    glNamedBufferStorage(id_, size_, data, glflags);
    if (storageFlags & BufferStorageFlag::MAP_MEMORY)
    {
      // GL_MAP_UNSYNCHRONIZED_BIT should be used if the user can map and unmap buffers at their own will
      constexpr GLenum access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
      mappedMemory_ = glMapNamedBufferRange(id_, 0, size_, access);
    }

    if (!name.empty())
    {
      glObjectLabel(GL_BUFFER, id_, static_cast<GLsizei>(name.length()), name.data());
    }

    detail::InvokeVerboseMessageCallback("Created buffer with handle ", id_);
  }

  Buffer::Buffer(size_t size, BufferStorageFlags storageFlags, std::string_view name)
    : Buffer(nullptr, size, storageFlags, name)
  {
  }

  Buffer::Buffer(TriviallyCopyableByteSpan data, BufferStorageFlags storageFlags, std::string_view name)
    : Buffer(data.data(), data.size_bytes(), storageFlags, name)
  {
  }

  Buffer::Buffer(Buffer&& old) noexcept
    : size_(std::exchange(old.size_, 0)),
      storageFlags_(std::exchange(old.storageFlags_, BufferStorageFlag::NONE)),
      id_(std::exchange(old.id_, 0)),
      mappedMemory_(std::exchange(old.mappedMemory_, nullptr))
  {
  }

  Buffer& Buffer::operator=(Buffer&& old) noexcept
  {
    if (&old == this)
      return *this;
    this->~Buffer();
    return *new (this) Buffer(std::move(old));
  }

  Buffer::~Buffer()
  {
    if (id_)
    {
      detail::InvokeVerboseMessageCallback("Destroyed buffer with handle ", id_);

      if (mappedMemory_)
      {
        glUnmapNamedBuffer(id_);
      }
      glDeleteBuffers(1, &id_);
    }
  }

  void Buffer::UpdateData(TriviallyCopyableByteSpan data, size_t destOffsetBytes)
  {
    UpdateData(data.data(), data.size_bytes(), destOffsetBytes);
  }

  void Buffer::UpdateData(const void* data, size_t size, size_t offset)
  {
    FWOG_ASSERT((storageFlags_ & BufferStorageFlag::DYNAMIC_STORAGE) &&
                "UpdateData can only be called on buffers created with the DYNAMIC_STORAGE flag");
    FWOG_ASSERT(size + offset <= Size());
    glNamedBufferSubData(id_, static_cast<GLuint>(offset), static_cast<GLuint>(size), data);
  }

  void Buffer::FillData(const BufferFillInfo& clear)
  {
    const auto actualSize = clear.size == WHOLE_BUFFER ? size_ : clear.size;
    FWOG_ASSERT(actualSize % 4 == 0 && "Size must be a multiple of 4 bytes");
    glClearNamedBufferSubData(id_,
                              GL_R32UI,
                              clear.offset,
                              actualSize,
                              GL_RED_INTEGER,
                              GL_UNSIGNED_INT,
                              &clear.data);
  }

  void Buffer::Invalidate()
  {
    glInvalidateBufferData(id_);
  }
} // namespace Fwog
