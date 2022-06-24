#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <fwog/BasicTypes.h>

namespace Fwog
{
  namespace detail
  {
    class SamplerCache;
  }

  class TextureView;

  struct TextureCreateInfo
  {
    ImageType imageType = {};
    Format format = {};
    Extent3D extent = {};
    uint32_t mipLevels = 0;
    uint32_t arrayLayers = 0;
    SampleCount sampleCount = {};
  };

  struct TextureViewCreateInfo
  {
    ImageType viewType = {};
    Format format = {};
    uint32_t minLevel = 0;
    uint32_t numLevels = 0;
    uint32_t minLayer = 0;
    uint32_t numLayers = 0;
  };

  struct TextureUpdateInfo
  {
    UploadDimension dimension = {};
    uint32_t level = 0;
    Extent3D offset = {};
    Extent3D size = {};
    UploadFormat format = {};
    UploadType type = {};
    const void* pixels = nullptr;
  };

  struct TextureClearInfo
  {
    uint32_t level = 0;
    Extent3D offset = {};
    Extent3D size = {};
    UploadFormat format = {};
    UploadType type = {};
    const void* data = nullptr;
  };

  struct SamplerState
  {
    bool operator==(const SamplerState& rhs) const noexcept = default;

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

  class Texture
  {
  public:
    [[nodiscard]] static std::optional<Texture> Create(const TextureCreateInfo& createInfo, std::string_view name = "");
    Texture(Texture&& old) noexcept;
    Texture& operator=(Texture&& old) noexcept;
    ~Texture();

    void SubImage(const TextureUpdateInfo& info);
    void ClearImage(const TextureClearInfo& info);
    void GenMipmaps();
    [[nodiscard]] uint32_t Handle() const { return id_; }
    [[nodiscard]] std::optional<TextureView> View() const; // TODO: rename to CreateView
    [[nodiscard]] std::optional<TextureView> MipView(uint32_t level) const; // TODO: rename to CreateMipView
    //[[nodiscard]] std::optional<TextureView> LayerView(uint32_t layer) const; // TODO: rename to CreateLayerView
    [[nodiscard]] const TextureCreateInfo& CreateInfo() const { return createInfo_; }
    [[nodiscard]] Extent3D Extent() const { return createInfo_.extent; }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

  protected:
    friend class TextureView; // TODO: remove
    Texture() {};
    uint32_t id_{};
    TextureCreateInfo createInfo_{};
  };

  class TextureView : public Texture
  {
  public:
    // make a texture view with explicit parameters
    [[nodiscard]] static std::optional<TextureView> Create(const TextureViewCreateInfo& viewInfo, const Texture& texture, std::string_view name = "");
    [[nodiscard]] static std::optional<TextureView> Create(const TextureViewCreateInfo& viewInfo, const TextureView& textureView, std::string_view name = "");

    // make a texture view with automatic parameters (view of whole texture, same type)
    [[nodiscard]] static std::optional<TextureView> Create(const Texture& texture, std::string_view name = "");

    TextureView(TextureView&& old) noexcept;
    TextureView& operator=(TextureView&& old) noexcept;
    ~TextureView();

    [[nodiscard]] TextureViewCreateInfo ViewInfo() const { return viewInfo_; }
    
    TextureView(const TextureView& other) = delete;
    TextureView& operator=(const TextureView& other) = delete;

  private:
    friend class Texture; // TODO: remove
    TextureView() {};
    TextureViewCreateInfo viewInfo_{};
  };

  // trivially copy + move constructible
  class Sampler
  {
  public:
    [[nodiscard]] static std::optional<Sampler> Create(const SamplerState& samplerState);
    [[nodiscard]] uint32_t Handle() const { return id_; }

  private:
    friend class detail::SamplerCache;
    Sampler() {}; // you cannot create samplers out of thin air
    explicit Sampler(uint32_t id) : id_(id) {};

    uint32_t id_{};
  };

  // convenience functions
  std::optional<Texture> CreateTexture2D(Extent2D size, Format format, std::string_view name = "");
  std::optional<Texture> CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name = "");
}