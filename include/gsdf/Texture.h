#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include "BasicTypes.h"

namespace GFX
{
  namespace detail
  {
    class SamplerCache;
  }

  class TextureSampler;
  class Texture;

  struct TextureCreateInfo
  {
    ImageType imageType{};
    Format format{};
    Extent3D extent{};
    uint32_t mipLevels{};
    uint32_t arrayLayers{};
    SampleCount sampleCount{};
  };

  struct TextureViewCreateInfo
  {
    ImageType viewType{};
    Format format{};
    uint32_t minLevel{};
    uint32_t numLevels{};
    uint32_t minLayer{};
    uint32_t numLayers{};
  };

  struct TextureUpdateInfo
  {
    UploadDimension dimension{};
    uint32_t level{};
    Extent3D offset{};
    Extent3D size{};
    UploadFormat format{};
    UploadType type{};
    void* pixels{};
  };

  struct SamplerState
  {
    bool operator==(const SamplerState& rhs) const;

    SamplerState() {};

    float lodBias{ 0 };
    float minLod{ -1000 };
    float maxLod{ 1000 };

    Filter minFilter         = Filter::LINEAR;
    Filter magFilter         = Filter::LINEAR;
    Filter mipmapFilter      = Filter::NONE;
    AddressMode addressModeU = AddressMode::CLAMP_TO_EDGE;
    AddressMode addressModeV = AddressMode::CLAMP_TO_EDGE;
    AddressMode addressModeW = AddressMode::CLAMP_TO_EDGE;
    BorderColor borderColor  = BorderColor::INT_OPAQUE_WHITE;
    SampleCount anisotropy   = SampleCount::SAMPLES_1;
    bool compareEnable       = false;
    CompareOp compareOp      = CompareOp::NEVER;
  };

  // serves as lightweight view of an image, cheap to construct, copy, and meant to be passed around
  class TextureView
  {
  public:
    // make a texture view with explicit parameters
    [[nodiscard]] static std::optional<TextureView> Create(const TextureViewCreateInfo& createInfo, const Texture& texture, std::string_view name = "");

    // make a texture view with automatic parameters (view of whole texture, same type)
    [[nodiscard]] static std::optional<TextureView> Create(const Texture& texture, std::string_view name = "");

    TextureView(const TextureView& other);
    TextureView(TextureView&& old) noexcept;
    TextureView& operator=(const TextureView& other);
    TextureView& operator=(TextureView&& old) noexcept;
    ~TextureView();

    void SubImage(const TextureUpdateInfo& info) const;
    [[nodiscard]] uint32_t Handle() const { return id_; }
    [[nodiscard]] TextureViewCreateInfo CreateInfo() const { return createInfo_; }
    [[nodiscard]] Extent3D Extent() const { return extent_; }
    //[[nodiscard]] std::optional<TextureView> MipView(uint32_t level) const;

  private:
    friend class Framebuffer;
    friend class Texture;
    static std::optional<TextureView> Create(const TextureViewCreateInfo& createInfo, uint32_t texture, Extent3D extent, std::string_view name = "");
    TextureView() {};
    uint32_t id_{};
    TextureViewCreateInfo createInfo_{};
    Extent3D extent_{};
  };

  // serves as the physical storage for textures
  class Texture
  {
  public:
    [[nodiscard]] static std::optional<Texture> Create(const TextureCreateInfo& createInfo, std::string_view name = "");
    Texture(Texture&& old) noexcept;
    Texture& operator=(Texture&& old) noexcept;
    ~Texture();

    void SubImage(const TextureUpdateInfo& info);
    void GenMipmaps();
    [[nodiscard]] uint32_t Handle() const { return id_; }
    [[nodiscard]] std::optional<TextureView> View() const;
    [[nodiscard]] std::optional<TextureView> MipView(uint32_t level) const;
    [[nodiscard]] const TextureCreateInfo& CreateInfo() const { return createInfo_; }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

  private:
    friend class TextureView;
    Texture() {};
    uint32_t id_{};
    TextureCreateInfo createInfo_{};
  };

  // trivially copy + move constructible
  class TextureSampler
  {
  public:
    [[nodiscard]] static std::optional<TextureSampler> Create(const SamplerState& samplerState);
    [[nodiscard]] uint32_t Handle() const { return id_; }

  private:
    friend class detail::SamplerCache;

    TextureSampler() {}; // you cannot create samplers out of thin air
    explicit TextureSampler(uint32_t id) : id_(id) {};

    uint32_t id_{};
  };

  // unsafe way to bind texture view and sampler using only API handles
  void BindTextureViewNative(uint32_t slot, uint32_t textureViewAPIHandle, uint32_t samplerAPIHandle);
  
  void BindTextureView(uint32_t slot, const TextureView& textureView, const TextureSampler& textureSampler);
  void UnbindTextureView(uint32_t slot);

  void BindImage(uint32_t slot, const TextureView& textureView, uint32_t level);

  // convenience functions
  std::optional<Texture> CreateTexture2D(Extent2D size, Format format, std::string_view name = "");
  std::optional<Texture> CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name = "");
}