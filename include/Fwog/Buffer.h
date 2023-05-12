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

  struct BufferClearInfo
  {
    uint64_t offset = 0;
    uint64_t size = WHOLE_BUFFER;
    Format internalFormat;
    UploadFormat uploadFormat = UploadFormat::INFER_FORMAT;
    UploadType uploadType = UploadType::INFER_TYPE;
    const void* data = nullptr;
  };

  enum class BufferStorageFlag : uint32_t
  {
    NONE = 0,

    /// @brief Allows the user to update the buffer's contents with Buffer::UpdateData
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
    
    Buffer(Buffer&& old) noexcept;
    Buffer& operator=(Buffer&& old) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer();

    void UpdateData(TriviallyCopyableByteSpan data, size_t destOffsetBytes = 0);

    void ClearSubData(const BufferClearInfo& clear);

    /// @brief Gets a pointer that is mapped to the buffer's data store
    /// @return A pointer to mapped memory if the buffer was created with BufferStorageFlag::MAP_MEMORY, otherwise nullptr
    [[nodiscard]] void* GetMappedPointer() noexcept
    {
      return mappedMemory_;
    }

    [[nodiscard]] const void* GetMappedPointer() const noexcept
    {
      return mappedMemory_;
    }

    [[nodiscard]] auto Handle() const noexcept
    {
      return id_;
    }

    [[nodiscard]] auto Size() const noexcept
    {
      return size_;
    }

    [[nodiscard]] bool IsMapped() const noexcept
    {
      return mappedMemory_ != nullptr;
    }

    /// @brief Invalidates the content of the buffer's data store
    ///
    /// This call can be used to optimize driver synchronization in certain cases.
    void Invalidate();

  protected:
    Buffer(const void* data, size_t size, BufferStorageFlags storageFlags);

    void UpdateData(const void* data, size_t size, size_t offset = 0);

    size_t size_{};
    BufferStorageFlags storageFlags_{};
    uint32_t id_{};
    void* mappedMemory_{};
  };

  /// @brief A buffer that provides type-safe operations
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

    void UpdateData(const T& data, size_t startIndex = 0)
    {
      Buffer::UpdateData(data, sizeof(T) * startIndex);
    }

    void UpdateData(std::span<const T> data, size_t startIndex = 0)
    {
      Buffer::UpdateData(data, sizeof(T) * startIndex);
    }

    void UpdateData(TriviallyCopyableByteSpan data, size_t destOffsetBytes = 0) = delete;

    [[nodiscard]] T* GetMappedPointer() noexcept
    {
      return static_cast<T*>(mappedMemory_);
    }

    [[nodiscard]] const T* GetMappedPointer() const noexcept
    {
      return static_cast<T*>(mappedMemory_);
    }

  private:
  };
} // namespace Fwog
