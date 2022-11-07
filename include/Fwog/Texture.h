#pragma once
#include <Fwog/BasicTypes.h>
#include <cstdint>
#include <string_view>

namespace Fwog
{
  namespace detail
  {
    class SamplerCache;
  }

  class TextureView;
  class Sampler;

  struct TextureCreateInfo
  {
    ImageType imageType = {};
    Format format = {};
    Extent3D extent = {};
    uint32_t mipLevels = 0;
    uint32_t arrayLayers = 0;
    SampleCount sampleCount = {};

    bool operator==(const TextureCreateInfo&) const noexcept = default;
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

    float lodBias{0};
    float minLod{-1000};
    float maxLod{1000};

    Filter minFilter = Filter::LINEAR;
    Filter magFilter = Filter::LINEAR;
    Filter mipmapFilter = Filter::NONE;
    AddressMode addressModeU = AddressMode::CLAMP_TO_EDGE;
    AddressMode addressModeV = AddressMode::CLAMP_TO_EDGE;
    AddressMode addressModeW = AddressMode::CLAMP_TO_EDGE;
    BorderColor borderColor = BorderColor::FLOAT_OPAQUE_WHITE;
    SampleCount anisotropy = SampleCount::SAMPLES_1;
    bool compareEnable = false;
    CompareOp compareOp = CompareOp::NEVER;
  };

  class Texture
  {
  public:
    explicit Texture(const TextureCreateInfo& createInfo, std::string_view name = "");
    Texture(Texture&& old) noexcept;
    Texture& operator=(Texture&& old) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    virtual ~Texture();

    bool operator==(const Texture&) const noexcept = default;

    void SubImage(const TextureUpdateInfo& info);
    void ClearImage(const TextureClearInfo& info);
    void GenMipmaps();

    // create a view of a single mip or layer of this texture
    [[nodiscard]] TextureView CreateMipView(uint32_t level) const;
    [[nodiscard]] TextureView CreateLayerView(uint32_t layer) const;
    [[nodiscard]] uint64_t GetBindlessHandle(Sampler sampler);

    [[nodiscard]] const TextureCreateInfo& CreateInfo() const
    {
      return createInfo_;
    }
    [[nodiscard]] Extent3D Extent() const
    {
      return createInfo_.extent;
    }
    [[nodiscard]] uint32_t Handle() const
    {
      return id_;
    }

  protected:
    Texture();
    uint32_t id_{};
    TextureCreateInfo createInfo_{};
    uint64_t bindlessHandle_ = 0;
  };

  // TODO: implement
  //class ColorTexture : public Texture
  //{
  //public:
  //  // Should this constructor take a version of TextureCreateInfo that uses a more constrained format enum?
  //  explicit ColorTexture()
  //};

  //class DepthStencilTexture : public Texture
  //{
  //public:
  //  // See comment for above class' constructor
  //  explicit DepthStencilTexture()
  //};

  class TextureView : public Texture
  {
  public:
    // make a texture view with explicit parameters
    explicit TextureView(const TextureViewCreateInfo& viewInfo, const Texture& texture, std::string_view name = "");
    explicit TextureView(const TextureViewCreateInfo& viewInfo,
                         const TextureView& textureView,
                         std::string_view name = "");

    // make a texture view with automatic parameters (view of whole texture, same type)
    explicit TextureView(const Texture& texture, std::string_view name = "");

    TextureView(TextureView&& old) noexcept;
    TextureView& operator=(TextureView&& old) noexcept;
    TextureView(const TextureView& other) = delete;
    TextureView& operator=(const TextureView& other) = delete;
    ~TextureView();

    [[nodiscard]] TextureViewCreateInfo ViewInfo() const
    {
      return viewInfo_;
    }

  private:
    TextureView();
    TextureViewCreateInfo viewInfo_{};
  };

  class Sampler
  {
  public:
    explicit Sampler(const SamplerState& samplerState);

    [[nodiscard]] uint32_t Handle() const
    {
      return id_;
    }

  private:
    friend class detail::SamplerCache;
    Sampler(){}; // you cannot create samplers out of thin air
    explicit Sampler(uint32_t id) : id_(id){};

    uint32_t id_{};
  };

  // convenience functions
  Texture CreateTexture2D(Extent2D size, Format format, std::string_view name = "");
  Texture CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name = "");
} // namespace Fwog