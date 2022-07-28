#pragma once
#include <Fwog/detail/Flags.h>
#include <Fwog/BasicTypes.h>
#include <span>
#include <type_traits>

namespace Fwog
{
  // used to constrain types accepted by Buffer
  class TriviallyCopyableByteSpan : public std::span<const std::byte>
  {
  public:
    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(const T& t) : std::span<const std::byte>(std::as_bytes(std::span{ &t, (size_t)1 })) {}

    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<const T> t) : std::span<const std::byte>(std::as_bytes(t)) {}
    
    template<typename T> requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<T> t) : std::span<const std::byte>(std::as_bytes(t)) {}
  };

  enum class BufferFlag : uint32_t
  {
    NONE =            0,
    DYNAMIC_STORAGE = 1 << 0,
    CLIENT_STORAGE =  1 << 1,

    MAP_READ =        1 << 2,
    MAP_WRITE =       1 << 3,
    MAP_PERSISTENT =  1 << 4,
    MAP_COHERENT =    1 << 5,
  };
  FWOG_DECLARE_FLAG_TYPE(BufferFlags, BufferFlag, uint32_t)

  class Buffer
  {
  public:
    explicit Buffer(size_t size, BufferFlags flags = BufferFlag::NONE);
    explicit Buffer(TriviallyCopyableByteSpan data, BufferFlags flags = BufferFlag::NONE);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer();

    void SubData(TriviallyCopyableByteSpan data, size_t destOffsetBytes) const;
    void ClearSubData(size_t offset, 
      size_t size, 
      Format internalFormat, 
      UploadFormat uploadFormat, 
      UploadType uploadType, 
      const void* data) const;

    // TODO: add range and read/write flags
    [[nodiscard]] void* Map() const;
    void Unmap() const;

    [[nodiscard]] auto Handle() const { return id_; }
    [[nodiscard]] auto Size() const { return size_; }
    [[nodiscard]] bool IsMapped() const { return isMapped_; }

  protected:
    Buffer() {}
    Buffer(const void* data, size_t size, BufferFlags flags);

    void SubData(const void* data, size_t size, size_t offset = 0) const;

    uint32_t id_{};
    size_t size_{};
    mutable bool isMapped_{ false };
  };

  template<class T> requires(std::is_trivially_copyable_v<T>)
  class TypedBuffer : public Buffer
  {
  public:
    explicit TypedBuffer(BufferFlags flags = BufferFlag::NONE)
      : Buffer(sizeof(T), flags) {}
    explicit TypedBuffer(size_t count, BufferFlags flags = BufferFlag::NONE)
      : Buffer(sizeof(T)* count, flags) {}
    explicit TypedBuffer(std::span<const T> data, BufferFlags flags = BufferFlag::NONE)
      : Buffer(data, flags) {}
    TypedBuffer(TypedBuffer&& other) noexcept = default;
    TypedBuffer& operator=(TypedBuffer&& other) noexcept = default;
    TypedBuffer(const TypedBuffer& other) = delete;
    TypedBuffer& operator=(const TypedBuffer&) = delete;

    void SubDataTyped(const T& data, size_t startIndex = 0) const
    {
      Buffer::SubData(data, sizeof(T) * startIndex);
    }

    void SubDataTyped(std::span<const T> data, size_t startIndex = 0) const
    {
      Buffer::SubData(data, sizeof(T) * startIndex);
    }

    [[nodiscard]] T* MapTyped() const
    {
      return reinterpret_cast<T*>(Buffer::Map());
    }

  private:
  };
}
