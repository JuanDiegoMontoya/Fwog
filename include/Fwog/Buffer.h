#pragma once
#include <Fwog/Config.h>
#include <Fwog/BasicTypes.h>
#include <Fwog/detail/Flags.h>
#include <span>
#include <type_traits>

namespace Fwog
{
  /// @brief Used to constrain the types accpeted by Buffer
  class TriviallyCopyableByteSpan : public std::span<const std::byte>
  {
  public:
    template<typename T>
      requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(const T& t)
      : std::span<const std::byte>(std::as_bytes(std::span{&t, static_cast<size_t>(1)}))
    {
    }

    template<typename T>
      requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<const T> t) : std::span<const std::byte>(std::as_bytes(t))
    {
    }

    template<typename T>
      requires std::is_trivially_copyable_v<T>
    TriviallyCopyableByteSpan(std::span<T> t) : std::span<const std::byte>(std::as_bytes(t))
    {
    }
  };

  enum class BufferStorageFlag : uint32_t
  {
    NONE = 0,

    /// @brief Allows the user to update the buffer's contents with Buffer::SubData
    DYNAMIC_STORAGE = 1 << 0,

    /// @brief Hints to the implementation to place the buffer storage in host memory
    CLIENT_STORAGE = 1 << 1,

    /// @brief Maps the buffer (persistently and coherently) upon creation
    MAP_MEMORY = 1 << 2,
  };
  FWOG_DECLARE_FLAG_TYPE(BufferStorageFlags, BufferStorageFlag, uint32_t)

  /// @brief Encapsulates an OpenGL buffer
  class Buffer
  {
  public:
    explicit Buffer(size_t size, BufferStorageFlags storageFlags = BufferStorageFlag::NONE);
    explicit Buffer(TriviallyCopyableByteSpan data, BufferStorageFlags storageFlags = BufferStorageFlag::NONE);

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer();

    void SubData(TriviallyCopyableByteSpan data, size_t destOffsetBytes = 0) const;
    void ClearSubData(size_t offset,
                      size_t size,
                      Format internalFormat,
                      UploadFormat uploadFormat,
                      UploadType uploadType,
                      const void* data) const;

    /// @brief Gets a pointer that is mapped to the buffer's data store
    /// @return A pointer to mapped memory if the buffer was created with BufferStorageFlag::MAP_MEMORY, otherwise nullptr
    [[nodiscard]] void* GetMappedPointer() const
    {
      return mappedMemory_;
    }

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
      return mappedMemory_ != nullptr;
    }

    /// @brief Invalidates the content of the buffer's data store
    ///
    /// This call can be used to optimize driver synchronization in certain cases.
    void Invalidate() const;

  protected:
    Buffer(const void* data, size_t size, BufferStorageFlags storageFlags);

    void SubData(const void* data, size_t size, size_t offset = 0) const;

    size_t size_{};
    BufferStorageFlags storageFlags_{};
    uint32_t id_{};
    void* mappedMemory_{};
  };

  /// @brief A buffer that provides typed operations
  /// @tparam T A trivially copyable type
  template<class T>
    requires(std::is_trivially_copyable_v<T>)
  class TypedBuffer : public Buffer
  {
  public:
    explicit TypedBuffer(BufferStorageFlags storageFlags = BufferStorageFlag::NONE) : Buffer(sizeof(T), storageFlags) {}
    explicit TypedBuffer(size_t count, BufferStorageFlags storageFlags = BufferStorageFlag::NONE)
      : Buffer(sizeof(T) * count, storageFlags)
    {
    }
    explicit TypedBuffer(std::span<const T> data, BufferStorageFlags storageFlags = BufferStorageFlag::NONE)
      : Buffer(data, storageFlags)
    {
    }
    explicit TypedBuffer(const T& data, BufferStorageFlags storageFlags = BufferStorageFlag::NONE)
      : Buffer(&data, sizeof(T), storageFlags)
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

    [[nodiscard]] void* GetMappedPointer() const = delete;

    [[nodiscard]] T* GetMappedPointerTyped() const
    {
      return reinterpret_cast<T*>(mappedMemory_);
    }

  private:
  };
} // namespace Fwog
