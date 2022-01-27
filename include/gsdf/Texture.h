#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include "BasicTypes.h"

namespace GFX
{
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
    SamplerState() {};
    union
    {
      struct
      {
        Filter magFilter : 2 = Filter::LINEAR;
        Filter minFilter : 2 = Filter::LINEAR;
        Filter mipmapFilter : 2 = Filter::NONE;
        AddressMode addressModeU : 3 = AddressMode::CLAMP_TO_EDGE;
        AddressMode addressModeV : 3 = AddressMode::CLAMP_TO_EDGE;
        AddressMode addressModeW : 3 = AddressMode::CLAMP_TO_EDGE;
        BorderColor borderColor : 3 = BorderColor::INT_OPAQUE_WHITE;
        Anisotropy anisotropy : 3 = Anisotropy::SAMPLES_1;
      }asBitField{};
      uint32_t asUint32;
    };

    float lodBias{ 0 };
    float minLod{ -1000 };
    float maxLod{ 1000 };
    // TODO: maybe add this later
    //CompareOp compareOp;
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
    [[nodiscard]] uint32_t GetAPIHandle() const { return id_; }
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
  // cannot be used directly for samplers
  class Texture
  {
  public:
    [[nodiscard]] static std::optional<Texture> Create(const TextureCreateInfo& createInfo, std::string_view name = "");
    Texture(Texture&& old) noexcept;
    Texture& operator=(Texture&& old) noexcept;
    ~Texture();

    void SubImage(const TextureUpdateInfo& info);
    void GenMipmaps();
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

  // stores texture sampling parameters
  // copy + move constructible
  class TextureSampler
  {
  public:
    [[nodiscard]] static std::optional<TextureSampler> Create(const SamplerState& initialState, std::string_view name = "");
    TextureSampler(const TextureSampler& other);
    TextureSampler(TextureSampler&& old) noexcept;
    TextureSampler& operator=(const TextureSampler& other);
    TextureSampler& operator=(TextureSampler&& old) noexcept;
    ~TextureSampler();

    void SetState(const SamplerState& samplerState);
    [[nodiscard]] const SamplerState& GetState() const noexcept { return samplerState_; }
    [[nodiscard]] uint32_t GetAPIHandle() const { return id_; }

  private:
    TextureSampler() {};
    void SetState(const SamplerState& samplerState, bool force);

    uint32_t id_{};
    SamplerState samplerState_{};
  };

  // unsafe way to bind texture view and sampler using only API handles
  void BindTextureViewNative(uint32_t slot, uint32_t textureViewAPIHandle, uint32_t samplerAPIHandle);
  
  void BindTextureView(uint32_t slot, const TextureView& textureView, const TextureSampler& textureSampler);
  void UnbindTextureView(uint32_t slot);

  void BindImage(uint32_t slot, const TextureView& textureView, uint32_t level);

  // convenience function
  std::optional<Texture> CreateTexture2D(Extent2D size, Format format, std::string_view name = "");
  std::optional<Texture> CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name = "");
}