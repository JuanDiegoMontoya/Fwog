#pragma once
#include <gsdf/Flags.h>
#include <optional>
#include <span>

namespace GFX
{
  enum class Target
  {
    VERTEX_BUFFER,
    SHADER_STORAGE_BUFFER,
    ATOMIC_BUFFER,
    DRAW_INDIRECT_BUFFER,
    PARAMETER_BUFFER,
    UNIFORM_BUFFER,
  };

  enum class BufferFlag : uint32_t
  {
    NONE = 1 << 0,
    DYNAMIC_STORAGE = 1 << 1,
    CLIENT_STORAGE = 1 << 2,

    MAP_READ = 1 << 3,
    MAP_WRITE = 1 << 4,
    MAP_PERSISTENT = 1 << 5,
    MAP_COHERENT = 1 << 6,
  };
  DECLARE_FLAG_TYPE(BufferFlags, BufferFlag, uint32_t)

  // general-purpose immutable graphics buffer storage
  class Buffer
  {
  public:
    [[nodiscard]] static std::optional<Buffer> Create(size_t size, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(nullptr, size, flags);
    }

    template<typename T>
    [[nodiscard]] static std::optional<Buffer> Create(std::span<T> data, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(data.data(), data.size_bytes(), flags);
    }

    // copies another buffer's data store and contents
    Buffer(const Buffer& other) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer();

    // for binding everything EXCEPT SSBOs and UBOs
    template<Target T>
    void Bind()
    {
      static_assert(T != Target::SHADER_STORAGE_BUFFER && T != Target::UNIFORM_BUFFER, "SSBO and UBO targets require an index.");
      BindBuffer(static_cast<uint32_t>(T));
    }

    // for binding SSBOs and UBOs
    template<Target T>
    void Bind(uint32_t index)
    {
      static_assert(T == Target::SHADER_STORAGE_BUFFER || T == Target::UNIFORM_BUFFER, "Only SSBO and UBO targets use an index.");
      BindBuffer(static_cast<uint32_t>(T));
      BindBufferBase(static_cast<uint32_t>(T), index);
    }

    template<typename T>
    void SubData(std::span<T> data, size_t destOffsetBytes)
    {
      SubData(data.data(), data.size_bytes(), destOffsetBytes);
    }

    // returns persistently mapped read/write pointer
    [[nodiscard]] void* GetMappedPointer();

    void UnmapPointer();

    [[nodiscard]] bool IsMapped() { return isMapped_; }

    // gets the OpenGL handle of this object
    [[nodiscard]] auto GetAPIHandle() const { return id_; }

    [[nodiscard]] auto Size() const { return size_; }

  private:
    Buffer() {}
    void BindBuffer(uint32_t target);
    void BindBufferBase(uint32_t target, uint32_t slot);
    static std::optional<Buffer> CreateInternal(const void* data, size_t size, BufferFlags flags);

    // updates a subset of the buffer's data store
    void SubData(const void* data, size_t size, size_t offset = 0);

    uint32_t id_{};
    uint32_t size_{};
    bool isMapped_{ false };
  };
}