#include <gsdf/Common.h>
#include <gsdf/Buffer.h>

uint32_t getSetBit(uint32_t val, uint32_t bit)
{
  return val & (1 << bit) ? bit : 0;
}

namespace GFX
{
  namespace
  {
    GLenum targets[]
    {
      GL_ARRAY_BUFFER,
      GL_SHADER_STORAGE_BUFFER,
      GL_ATOMIC_COUNTER_BUFFER,
      GL_DRAW_INDIRECT_BUFFER,
      GL_PARAMETER_BUFFER,
      GL_UNIFORM_BUFFER,
    };

    GLbitfield bufferFlags[]
    {
      0,
      GL_DYNAMIC_STORAGE_BIT,
      GL_CLIENT_STORAGE_BIT,
      GL_MAP_READ_BIT,
      GL_MAP_WRITE_BIT,
      GL_MAP_PERSISTENT_BIT,
      GL_MAP_COHERENT_BIT,
    };
  }

  std::optional<Buffer> Buffer::CreateInternal(const void* data, size_t size, BufferFlags flags)
  {
    size = std::max(size, 1ull);
    GLbitfield glflags = 0;
    for (int i = 1; i < _countof(bufferFlags); i++)
      glflags |= bufferFlags[getSetBit((uint32_t)flags, i)];
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
    GSDF_ASSERT(!IsMapped() && "Buffers must not be mapped at time of destruction");
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
    isMapped_ = true;
    return glMapNamedBuffer(id_, GL_READ_WRITE);
  }

  void Buffer::UnmapPointer()
  {
    GSDF_ASSERT(IsMapped(), "Buffers that aren't mapped cannot be unmapped");
    isMapped_ = false;
    glUnmapNamedBuffer(id_);
  }

  void Buffer::BindBuffer(uint32_t target)
  {
    glBindBuffer(targets[target], id_);
  }

  void Buffer::BindBufferBase(uint32_t target, uint32_t slot)
  {
    glBindBufferBase(targets[target], slot, id_);
  }
}