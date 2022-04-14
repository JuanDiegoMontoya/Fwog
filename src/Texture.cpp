#include <gsdf/Common.h>
#include <gsdf/detail/ApiToEnum.h>
#include <gsdf/detail/SamplerCache.h>
#include <gsdf/Texture.h>
#include <utility>
#include <array>

#define MAX_NAME_LEN 256

namespace GFX
{
  // static objects
  // TODO: move initialization
  namespace
  {
    detail::SamplerCache sSamplerCache;
  }

  namespace // detail
  {
    void subImage(uint32_t texture, const TextureUpdateInfo& info)
    {
      // TODO: safety checks

      switch (info.dimension)
      {
      case UploadDimension::ONE:
        glTextureSubImage1D(texture, info.level, info.offset.width, info.size.width, detail::UploadFormatToGL(info.format), detail::UploadTypeToGL(info.type), info.pixels);
        break;
      case UploadDimension::TWO:
        glTextureSubImage2D(texture, info.level, info.offset.width, info.offset.height, info.size.width, info.size.height, detail::UploadFormatToGL(info.format), detail::UploadTypeToGL(info.type), info.pixels);
        break;
      case UploadDimension::THREE:
        glTextureSubImage3D(texture, info.level, info.offset.width, info.offset.height, info.offset.depth, info.size.width, info.size.height, info.size.depth, detail::UploadFormatToGL(info.format), detail::UploadTypeToGL(info.type), info.pixels);
        break;
      }
    }
  }



  std::optional<Texture> Texture::Create(const TextureCreateInfo& createInfo, std::string_view name)
  {
    Texture texture;
    texture.createInfo_ = createInfo;
    glCreateTextures(detail::ImageTypeToGL(createInfo.imageType), 1, &texture.id_);

    switch (createInfo.imageType)
    {
    case ImageType::TEX_1D:
      glTextureStorage1D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width);
      break;
    case ImageType::TEX_2D:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height);
      break;
    case ImageType::TEX_3D:
      glTextureStorage3D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height, createInfo.extent.depth);
      break;
    case ImageType::TEX_1D_ARRAY:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.arrayLayers);
      break;
    case ImageType::TEX_2D_ARRAY:
      glTextureStorage3D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height, createInfo.arrayLayers);
      break;
    case ImageType::TEX_CUBEMAP:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height);
      break;
      //case ImageType::TEX_CUBEMAP_ARRAY:
      //  ASSERT(false);
      //  break;
    case ImageType::TEX_2D_MULTISAMPLE:
      glTextureStorage2DMultisample(texture.id_, detail::SampleCountToGL(createInfo.sampleCount), detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height, GL_FALSE);
      break;
    case ImageType::TEX_2D_MULTISAMPLE_ARRAY:
      glTextureStorage3DMultisample(texture.id_, detail::SampleCountToGL(createInfo.sampleCount), detail::FormatToGL(createInfo.format), createInfo.extent.width, createInfo.extent.height, createInfo.arrayLayers, GL_FALSE);
      break;
    default:
      break;
    }

    if (!name.empty())
    {
      glObjectLabel(GL_TEXTURE, texture.id_, static_cast<GLsizei>(name.length()), name.data());
    }
    return texture;
  }

  Texture::Texture(Texture&& old) noexcept
  {
    id_ = std::exchange(old.id_, 0);
    createInfo_ = old.createInfo_;
  }

  Texture& Texture::operator=(Texture&& old) noexcept
  {
    if (&old == this) return *this;
    this->~Texture();
    id_ = std::exchange(old.id_, 0);
    createInfo_ = old.createInfo_;
    return *this;
  }

  Texture::~Texture()
  {
    glDeleteTextures(1, &id_);
  }

  std::optional<TextureView> Texture::View() const
  {
    return TextureView::Create(*this);
  }

  std::optional<TextureView> Texture::MipView(uint32_t level) const
  {
    TextureViewCreateInfo createInfo
    {
      .viewType = createInfo_.imageType,
      .format = createInfo_.format,
      .minLevel = level,
      .numLevels = 1,
      .minLayer = 0,
      .numLayers = createInfo_.arrayLayers
    };
    return TextureView::Create(createInfo, id_, createInfo_.extent);
  }

  void Texture::SubImage(const TextureUpdateInfo& info)
  {
    subImage(id_, info);
  }

  void Texture::GenMipmaps()
  {
    glGenerateTextureMipmap(id_);
  }



  std::optional<TextureView> TextureView::Create(const TextureViewCreateInfo& createInfo, const Texture& texture, std::string_view name)
  {
    return Create(createInfo, texture.id_, texture.createInfo_.extent, name);
  }

  std::optional<TextureView> TextureView::Create(const TextureViewCreateInfo& createInfo, const TextureView& textureView, std::string_view name)
  {
    return Create(createInfo, textureView.id_, textureView.extent_, name);
  }

  std::optional<TextureView> TextureView::Create(const Texture& texture, std::string_view name)
  {
    TextureViewCreateInfo createInfo
    {
      .viewType = texture.createInfo_.imageType,
      .format = texture.createInfo_.format,
      .minLevel = 0,
      .numLevels = texture.createInfo_.mipLevels,
      .minLayer = 0,
      .numLayers = texture.createInfo_.arrayLayers
    };
    return Create(createInfo, texture, name);
  }

  std::optional<TextureView> TextureView::Create(const TextureViewCreateInfo& createInfo, uint32_t texture, Extent3D extent, std::string_view name)
  {
    TextureView view;
    view.createInfo_ = createInfo;
    view.extent_ = extent;
    glGenTextures(1, &view.id_); // glCreateTextures does not work here
    glTextureView(view.id_, detail::ImageTypeToGL(createInfo.viewType), texture,
      detail::FormatToGL(createInfo.format), createInfo.minLevel,
      createInfo.numLevels, createInfo.minLayer,
      createInfo.numLayers);
    if (!name.empty())
    {
      glObjectLabel(GL_TEXTURE, view.id_, static_cast<GLsizei>(name.length()), name.data());
    }
    return view;
  }

  TextureView::TextureView(const TextureView& other)
  {
    std::array<char, MAX_NAME_LEN> name{};
    GLsizei len{};
    glGetObjectLabel(GL_TEXTURE, other.id_, MAX_NAME_LEN, &len, name.data());
    *this = other;
    if (len > 0)
    {
      glObjectLabel(GL_TEXTURE, id_, len, name.data());
    }
  }

  TextureView::TextureView(TextureView&& old) noexcept
  {
    id_ = std::exchange(old.id_, 0);
    createInfo_ = old.createInfo_;
    extent_ = old.extent_;
  }

  TextureView& TextureView::operator=(const TextureView& other)
  {
    if (&other == this) return *this;
    *this = *Create(other.createInfo_, other.id_, other.extent_); // invokes move assignment
    return *this;
  }

  TextureView& TextureView::operator=(TextureView&& old) noexcept
  {
    if (&old == this) return *this;
    this->~TextureView();
    id_ = std::exchange(old.id_, 0);
    createInfo_ = old.createInfo_;
    extent_ = old.extent_;
    return *this;
  }

  TextureView::~TextureView()
  {
    glDeleteTextures(1, &id_);
  }

  void TextureView::SubImage(const TextureUpdateInfo& info) const
  {
    subImage(id_, info);
  }



  std::optional<TextureSampler> TextureSampler::Create(const SamplerState& samplerState)
  {
    return sSamplerCache.CreateOrGetCachedTextureSampler(samplerState);
  }

  void BindTextureViewNative(uint32_t slot, uint32_t textureViewAPIHandle, uint32_t samplerAPIHandle)
  {
    glBindTextureUnit(slot, textureViewAPIHandle);
    glBindSampler(slot, samplerAPIHandle);
  }

  void BindTextureView(uint32_t slot, const TextureView& textureView, const TextureSampler& textureSampler)
  {
    glBindTextureUnit(slot, textureView.Handle());
    glBindSampler(slot, textureSampler.Handle());
  }

  void UnbindTextureView(uint32_t slot)
  {
    glBindTextureUnit(slot, 0);
    glBindSampler(slot, 0);
  }

  void BindImage(uint32_t slot, const TextureView& textureView, uint32_t level)
  {
    GSDF_ASSERT(level < textureView.CreateInfo().numLevels);
    glBindImageTexture(slot, textureView.Handle(), level, GL_TRUE, 0,
      GL_READ_WRITE, detail::FormatToGL(textureView.CreateInfo().format));
  }

  std::optional<Texture> CreateTexture2D(Extent2D size, Format format, std::string_view name)
  {
    TextureCreateInfo createInfo
    {
      .imageType = ImageType::TEX_2D,
      .format = format,
      .extent = { size.width, size.height, 1 },
      .mipLevels = 1,
      .arrayLayers = 1,
      .sampleCount = SampleCount::SAMPLES_1
    };
    return Texture::Create(createInfo, name);
  }

  std::optional<Texture> CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name)
  {
    TextureCreateInfo createInfo
    {
      .imageType = ImageType::TEX_2D,
      .format = format,
      .extent = { size.width, size.height, 1 },
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .sampleCount = SampleCount::SAMPLES_1
    };
    return Texture::Create(createInfo, name);
  }

  bool SamplerState::operator==(const SamplerState& rhs) const
  {
    return 
      minFilter     == rhs.minFilter &&
      magFilter     == rhs.magFilter &&
      mipmapFilter  == rhs.mipmapFilter &&
      addressModeU  == rhs.addressModeU &&
      addressModeV  == rhs.addressModeV &&
      addressModeW  == rhs.addressModeW &&
      borderColor   == rhs.borderColor &&
      anisotropy    == rhs.anisotropy &&
      compareEnable == rhs.compareEnable &&
      compareOp     == rhs.compareOp &&
      lodBias       == rhs.lodBias &&
      minLod        == rhs.minLod &&
      maxLod        == rhs.maxLod;
  }
}