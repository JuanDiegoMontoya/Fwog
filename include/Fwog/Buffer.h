#pragma once
#include <Fwog/BasicTypes.h>
#include <Fwog/detail/Flags.h>
#include <span>
#include <type_traits>

namespace Fwog
{
  // clang-format off
  
  // used to constrain types accepted by Buffer
  class TriviallyCopyableByteSpan : public std::span<const std::byte>
  {
  public:
    template<typename T>
    requires std::is_trivially_copyable_v<T> TriviallyCopyableByteSpan(const T& t)
      : std::span<const std::byte>(std::as_bytes(std::span{&t, static_cast<size_t>(1)}))
    {
    }

    template<typename T>
    requires std::is_trivially_copyable_v<T> TriviallyCopyableByteSpan(std::span<const T> t)
      : std::span<const std::byte>(std::as_bytes(t))
    {
    }

    template<typename T>
    requires std::is_trivially_copyable_v<T> TriviallyCopyableByteSpan(std::span<T> t)
      : std::span<const std::byte>(std::as_bytes(t))
    {
    }
  };

  enum class BufferStorageFlag : uint32_t
  {
    NONE = 0,
    DYNAMIC_STORAGE = 1 << 0,
    CLIENT_STORAGE = 1 << 1,
  };
  FWOG_DECLARE_FLAG_TYPE(BufferStorageFlags, BufferStorageFlag, uint32_t)

  enum class BufferMapFlag : uint32_t
  {
    NONE = 0,
    MAP_READ = 1 << 0,
    MAP_WRITE = 1 << 1,
    MAP_PERSISTENT = 1 << 2,
    MAP_COHERENT = 1 << 3,
  };
  FWOG_DECLARE_FLAG_TYPE(BufferMapFlags, BufferMapFlag, uint32_t)

  class Buffer
  {
  public:
    explicit Buffer(size_t size,
                    BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                    BufferMapFlags mapFlags = BufferMapFlag::NONE);
    explicit Buffer(TriviallyCopyableByteSpan data,
                    BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                    BufferMapFlags mapFlags = BufferMapFlag::NONE);

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
    [[nodiscard]] void* Map(BufferMapFlags flags) const;
    void Unmap() const;

    [[nodiscard]] auto Handle() const
    {
      return id_;
    }
    [[nodiscard]] auto Size() const
    {
      return size_;
    }
    [[nodiscard]] bool IsMapped() const
    {
      return isMapped_;
    }

  protected:
    Buffer() {}
    Buffer(const void* data, size_t size, BufferStorageFlags storageFlags, BufferMapFlags mapFlags);

    void SubData(const void* data, size_t size, size_t offset = 0) const;

    size_t size_{};
    uint32_t id_{};
    mutable bool isMapped_{false};
  };

  template<class T>
  requires(std::is_trivially_copyable_v<T>) class TypedBuffer : public Buffer
  {
  public:
    explicit TypedBuffer(BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                         BufferMapFlags mapFlags = BufferMapFlag::NONE)
      : Buffer(sizeof(T), storageFlags, mapFlags)
    {
    }
    explicit TypedBuffer(size_t count,
                         BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                         BufferMapFlags mapFlags = BufferMapFlag::NONE)
      : Buffer(sizeof(T) * count, storageFlags, mapFlags)
    {
    }
    explicit TypedBuffer(std::span<const T> data,
                         BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                         BufferMapFlags mapFlags = BufferMapFlag::NONE)
      : Buffer(data, storageFlags, mapFlags)
    {
    }
    explicit TypedBuffer(const T& data,
                         BufferStorageFlags storageFlags = BufferStorageFlag::NONE,
                         BufferMapFlags mapFlags = BufferMapFlag::NONE)
      : Buffer(&data, sizeof(T), storageFlags, mapFlags)
    {
    }

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

    [[nodiscard]] T* MapTyped(BufferMapFlags flags) const
    {
      return reinterpret_cast<T*>(Buffer::Map(flags));
    }

  private:
  };

  // clang-format on
} // namespace Fwog
