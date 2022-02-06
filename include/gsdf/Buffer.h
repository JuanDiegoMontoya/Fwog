#pragma once
#include <gsdf/detail/Flags.h>
#include <optional>
#include <span>
#include <type_traits>

namespace GFX
{
  class cool_bytes : public std::span<const std::byte>
  {
  public:
    template<typename T> requires std::is_trivially_copyable_v<T>
    cool_bytes(const T& t) : std::span<const std::byte>(std::as_bytes(std::span{ &t, 1 })) {}

    template<typename T> requires std::is_trivially_copyable_v<T>
    cool_bytes(std::span<const T> t) : std::span<const std::byte>(std::as_bytes(t)) {}
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
  GSDF_DECLARE_FLAG_TYPE(BufferFlags, BufferFlag, uint32_t)

  // general-purpose immutable graphics buffer storage
  class Buffer
  {
  public:
    [[nodiscard]] static std::optional<Buffer> Create(size_t size, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(nullptr, size, flags);
    }

    [[nodiscard]] static std::optional<Buffer> Create(cool_bytes data, BufferFlags flags = BufferFlag::NONE)
    {
      return CreateInternal(data.data(), data.size_bytes(), flags);
    }

    // copies another buffer's data store and contents
    Buffer(const Buffer& other) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer();

    void SubData(cool_bytes data, size_t destOffsetBytes)
    {
      SubData(data.data(), data.size_bytes(), destOffsetBytes);
    }

    // returns persistently mapped read/write pointer
    [[nodiscard]] void* GetMappedPointer();

    void UnmapPointer();

    [[nodiscard]] bool IsMapped() { return isMapped_; }

    // gets the OpenGL handle of this object
    [[nodiscard]] auto Handle() const { return id_; }

    [[nodiscard]] auto Size() const { return size_; }

  private:
    Buffer() {}
    static std::optional<Buffer> CreateInternal(const void* data, size_t size, BufferFlags flags);

    // updates a subset of the buffer's data store
    void SubData(const void* data, size_t size, size_t offset = 0);

    uint32_t id_{};
    size_t size_{};
    bool isMapped_{ false };
  };
}