#include <gsdf/Common.h>
#include <gsdf/Texture.h>
#include <utility>

#define MAX_NAME_LEN 256

namespace GFX
{
  namespace // detail
  {
    static GLint targets[]
    {
      GL_TEXTURE_1D,
      GL_TEXTURE_2D,
      GL_TEXTURE_3D,
      GL_TEXTURE_1D_ARRAY,
      GL_TEXTURE_2D_ARRAY,
      GL_TEXTURE_CUBE_MAP,
      //GL_TEXTURE_CUBE_MAP_ARRAY,
      GL_TEXTURE_2D_MULTISAMPLE,
      GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    };

    static GLint formats[]
    {
      0,
      GL_R8,
      GL_R8_SNORM,
      GL_R16,
      GL_R16_SNORM,
      GL_RG8,
      GL_RG8_SNORM,
      GL_RG16,
      GL_RG16_SNORM,
      GL_R3_G3_B2,
      GL_RGB4,
      GL_RGB5,
      GL_RGB8,
      GL_RGB8_SNORM,
      GL_RGB10,
      GL_RGB12,
      GL_RGB16_SNORM,
      GL_RGBA2,
      GL_RGBA4,
      GL_RGB5_A1,
      GL_RGBA8,
      GL_RGBA8_SNORM,
      GL_RGB10_A2,
      GL_RGB10_A2UI,
      GL_RGBA12,
      GL_RGBA16,
      GL_SRGB8,
      GL_SRGB8_ALPHA8,
      GL_R16F,
      GL_RG16F,
      GL_RGB16F,
      GL_RGBA16F,
      GL_R32F,
      GL_RG32F,
      GL_RGB32F,
      GL_RGBA32F,
      GL_R11F_G11F_B10F,
      GL_RGB9_E5,
      GL_R8I,
      GL_R8UI,
      GL_R16I,
      GL_R16UI,
      GL_R32I,
      GL_R32UI,
      GL_RG8I,
      GL_RG8UI,
      GL_RG16I,
      GL_RG16UI,
      GL_RG32I,
      GL_RG32UI,
      GL_RGB8I,
      GL_RGB8UI,
      GL_RGB16I,
      GL_RGB16UI,
      GL_RGB32I,
      GL_RGB32UI,
      GL_RGBA8I,
      GL_RGBA8UI,
      GL_RGBA16I,
      GL_RGBA16UI,
      GL_RGBA32I,
      GL_RGBA32UI,

      GL_DEPTH_COMPONENT32F,
      GL_DEPTH_COMPONENT32,
      GL_DEPTH_COMPONENT24,
      GL_DEPTH_COMPONENT16,
      GL_DEPTH32F_STENCIL8,
      GL_DEPTH24_STENCIL8,
    };

    static GLint sampleCounts[]{ 1, 2, 4, 8, 16 };

    static GLint uploadFormats[]
    {
      0,
      GL_RED,
      GL_RG,
      GL_RGB,
      GL_BGR,
      GL_RGBA,
      GL_BGRA,
      GL_DEPTH_COMPONENT,
      GL_STENCIL_INDEX,
    };

    static GLint uploadTypes[]
    {
      0,
      GL_UNSIGNED_BYTE,
      GL_BYTE,
      GL_UNSIGNED_SHORT,
      GL_SHORT,
      GL_UNSIGNED_INT,
      GL_INT,
      GL_FLOAT,
      GL_UNSIGNED_BYTE_3_3_2,
      GL_UNSIGNED_BYTE_2_3_3_REV,
      GL_UNSIGNED_SHORT_5_6_5,
      GL_UNSIGNED_SHORT_5_6_5_REV,
      GL_UNSIGNED_SHORT_4_4_4_4,
      GL_UNSIGNED_SHORT_4_4_4_4_REV,
      GL_UNSIGNED_SHORT_5_5_5_1,
      GL_UNSIGNED_SHORT_1_5_5_5_REV,
      GL_UNSIGNED_INT_8_8_8_8,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      GL_UNSIGNED_INT_10_10_10_2,
      GL_UNSIGNED_INT_2_10_10_10_REV,
    };

    static GLint addressModes[]
    {
      GL_REPEAT,
      GL_MIRRORED_REPEAT,
      GL_CLAMP_TO_EDGE,
      GL_CLAMP_TO_BORDER,
      GL_MIRROR_CLAMP_TO_EDGE,
    };

    static GLfloat anisotropies[]{ 1, 2, 4, 8, 16 };

    void subImage(uint32_t texture, const TextureUpdateInfo& info)
    {
      // TODO: safety checks

      switch (info.dimension)
      {
      case UploadDimension::ONE:
        glTextureSubImage1D(texture, info.level, info.offset.width, info.size.width, uploadFormats[(int)info.format], uploadTypes[(int)info.type], info.pixels);
        break;
      case UploadDimension::TWO:
        glTextureSubImage2D(texture, info.level, info.offset.width, info.offset.height, info.size.width, info.size.height, uploadFormats[(int)info.format], uploadTypes[(int)info.type], info.pixels);
        break;
      case UploadDimension::THREE:
        glTextureSubImage3D(texture, info.level, info.offset.width, info.offset.height, info.offset.depth, info.size.width, info.size.height, info.size.depth, uploadFormats[(int)info.format], uploadTypes[(int)info.type], info.pixels);
        break;
      }
    }
  }



  std::optional<Texture> Texture::Create(const TextureCreateInfo& createInfo, std::string_view name)
  {
    Texture texture;
    texture.createInfo_ = createInfo;
    glCreateTextures(targets[(int)createInfo.imageType], 1, &texture.id_);

    switch (createInfo.imageType)
    {
    case ImageType::TEX_1D:
      glTextureStorage1D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width);
      break;
    case ImageType::TEX_2D:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height);
      break;
    case ImageType::TEX_3D:
      glTextureStorage3D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height, createInfo.extent.depth);
      break;
    case ImageType::TEX_1D_ARRAY:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width, createInfo.arrayLayers);
      break;
    case ImageType::TEX_2D_ARRAY:
      glTextureStorage3D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height, createInfo.arrayLayers);
      break;
    case ImageType::TEX_CUBEMAP:
      glTextureStorage2D(texture.id_, createInfo.mipLevels, formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height);
      break;
      //case ImageType::TEX_CUBEMAP_ARRAY:
      //  ASSERT(false);
      //  break;
    case ImageType::TEX_2D_MULTISAMPLE:
      glTextureStorage2DMultisample(texture.id_, sampleCounts[(int)createInfo.sampleCount], formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height, GL_FALSE);
      break;
    case ImageType::TEX_2D_MULTISAMPLE_ARRAY:
      glTextureStorage3DMultisample(texture.id_, sampleCounts[(int)createInfo.sampleCount], formats[(int)createInfo.format], createInfo.extent.width, createInfo.extent.height, createInfo.arrayLayers, GL_FALSE);
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
    glTextureView(view.id_, targets[(int)createInfo.viewType], texture,
      formats[(int)createInfo.format], createInfo.minLevel,
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
    char name[MAX_NAME_LEN]{};
    GLsizei len{};
    glGetObjectLabel(GL_TEXTURE, other.id_, MAX_NAME_LEN, &len, name);
    *this = other;
    if (len > 0)
    {
      glObjectLabel(GL_TEXTURE, id_, len, name);
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

  //std::optional<TextureView> TextureView::MipView(uint32_t level) const
  //{
  //  TextureViewCreateInfo createInfo
  //  {
  //    .viewType = createInfo_.imageType,
  //    .format = createInfo_.format,
  //    .minLevel = level,
  //    .numLevels = 1,
  //    .minLayer = 0,
  //    .numLayers = createInfo_.arrayLayers
  //  };
  //  return TextureView::Create(createInfo, id_, extent_);
  //}



  std::optional<TextureSampler> TextureSampler::Create(const SamplerState& state, std::string_view name)
  {
    TextureSampler sampler;
    glCreateSamplers(1, &sampler.id_);
    sampler.SetState(state, true);
    if (!name.empty())
    {
      glObjectLabel(GL_SAMPLER, sampler.id_, static_cast<GLsizei>(name.length()), name.data());
    }
    return sampler;
  }

  TextureSampler::TextureSampler(const TextureSampler& other)
  {
    char name[MAX_NAME_LEN]{};
    GLsizei len{};
    glGetObjectLabel(GL_SAMPLER, other.id_, MAX_NAME_LEN, &len, name);
    *this = other;
    if (len > 0)
    {
      glObjectLabel(GL_SAMPLER, id_, len, name);
    }
  }

  TextureSampler::TextureSampler(TextureSampler&& old) noexcept
  {
    id_ = std::exchange(old.id_, 0);
    samplerState_ = old.samplerState_;
  }

  TextureSampler& TextureSampler::operator=(const TextureSampler& other)
  {
    if (&other == this) return *this;
    *this = *Create(other.samplerState_); // invokes move assignment
    return *this;
  }

  TextureSampler& TextureSampler::operator=(TextureSampler&& old) noexcept
  {
    if (&old == this) return *this;
    this->~TextureSampler();
    id_ = std::exchange(old.id_, 0);
    samplerState_ = old.samplerState_;
    return *this;
  }

  TextureSampler::~TextureSampler()
  {
    glDeleteSamplers(1, &id_);
  }

  void TextureSampler::SetState(const SamplerState& state)
  {
    SetState(state, false);
  }

  void TextureSampler::SetState(const SamplerState& state, bool force)
  {
    // early out if the new state is equal to the previous
    if (state.asUint32 == samplerState_.asUint32 &&
      state.lodBias == samplerState_.lodBias &&
      state.minLod == samplerState_.minLod &&
      state.maxLod == samplerState_.maxLod &&
      !force)
    {
      return;
    }

    if (state.asBitField.magFilter != samplerState_.asBitField.magFilter || force)
    {
      GLint filter = state.asBitField.magFilter == Filter::LINEAR ? GL_LINEAR : GL_NEAREST;
      glSamplerParameteri(id_, GL_TEXTURE_MAG_FILTER, filter);
    }
    if (state.asBitField.minFilter != samplerState_.asBitField.minFilter ||
      state.asBitField.mipmapFilter != samplerState_.asBitField.mipmapFilter ||
      force)
    {
      GLint filter{};
      switch (state.asBitField.mipmapFilter)
      {
      case (Filter::NONE):
        filter = state.asBitField.minFilter == Filter::LINEAR ? GL_LINEAR : GL_NEAREST;
        break;
      case (Filter::NEAREST):
        filter = state.asBitField.minFilter == Filter::LINEAR ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
        break;
      case (Filter::LINEAR):
        filter = state.asBitField.minFilter == Filter::LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
        break;
      default: UNREACHABLE;
      }
      glSamplerParameteri(id_, GL_TEXTURE_MIN_FILTER, filter);
    }

    if (state.asBitField.addressModeU != samplerState_.asBitField.addressModeU || force)
      glSamplerParameteri(id_, GL_TEXTURE_WRAP_S, addressModes[(int)state.asBitField.addressModeU]);
    if (state.asBitField.addressModeV != samplerState_.asBitField.addressModeV || force)
      glSamplerParameteri(id_, GL_TEXTURE_WRAP_T, addressModes[(int)state.asBitField.addressModeV]);
    if (state.asBitField.addressModeW != samplerState_.asBitField.addressModeW || force)
      glSamplerParameteri(id_, GL_TEXTURE_WRAP_R, addressModes[(int)state.asBitField.addressModeW]);

    if (state.asBitField.borderColor != samplerState_.asBitField.borderColor || force)
    {
      // TODO: determine whether int white values should be 1 or 255
      switch (state.asBitField.borderColor)
      {
      case BorderColor::FLOAT_TRANSPARENT_BLACK:
      {
        constexpr GLfloat color[4]{ 0, 0, 0, 0 };
        glSamplerParameterfv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      case BorderColor::INT_TRANSPARENT_BLACK:
      {
        constexpr GLint color[4]{ 0, 0, 0, 0 };
        glSamplerParameteriv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      case BorderColor::FLOAT_OPAQUE_BLACK:
      {
        constexpr GLfloat color[4]{ 0, 0, 0, 1 };
        glSamplerParameterfv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      case BorderColor::INT_OPAQUE_BLACK:
      {
        constexpr GLint color[4]{ 0, 0, 0, 255 };
        glSamplerParameteriv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      case BorderColor::FLOAT_OPAQUE_WHITE:
      {
        constexpr GLfloat color[4]{ 1, 1, 1, 1 };
        glSamplerParameterfv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      case BorderColor::INT_OPAQUE_WHITE:
      {
        constexpr GLint color[4]{ 255, 255, 255, 255 };
        glSamplerParameteriv(id_, GL_TEXTURE_BORDER_COLOR, color);
        break;
      }
      default:
        UNREACHABLE;
        break;
      }
    }

    if (state.asBitField.anisotropy != samplerState_.asBitField.anisotropy || force)
      glSamplerParameterf(id_, GL_TEXTURE_MAX_ANISOTROPY, anisotropies[(int)state.asBitField.anisotropy]);

    if (state.lodBias != samplerState_.lodBias || force)
      glSamplerParameterf(id_, GL_TEXTURE_LOD_BIAS, state.lodBias);
    if (state.minLod != samplerState_.minLod || force)
      glSamplerParameterf(id_, GL_TEXTURE_MIN_LOD, state.minLod);
    if (state.maxLod != samplerState_.maxLod || force)
      glSamplerParameterf(id_, GL_TEXTURE_MAX_LOD, state.maxLod);

    samplerState_ = state;
  }

  void BindTextureViewNative(uint32_t slot, uint32_t textureViewAPIHandle, uint32_t samplerAPIHandle)
  {
    glBindTextureUnit(slot, textureViewAPIHandle);
    glBindSampler(slot, samplerAPIHandle);
  }

  void BindTextureView(uint32_t slot, const TextureView& textureView, const TextureSampler& textureSampler)
  {
    glBindTextureUnit(slot, textureView.GetAPIHandle());
    glBindSampler(slot, textureSampler.GetAPIHandle());
  }

  void UnbindTextureView(uint32_t slot)
  {
    glBindTextureUnit(slot, 0);
    glBindSampler(slot, 0);
  }

  void BindImage(uint32_t slot, const TextureView& textureView, uint32_t level)
  {
    GSDF_ASSERT(level < textureView.CreateInfo().numLevels);
    glBindImageTexture(slot, textureView.GetAPIHandle(), level, GL_TRUE, 0,
      GL_READ_WRITE, formats[(int)textureView.CreateInfo().format]);
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
      .sampleCount = SampleCount::ONE
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
      .sampleCount = SampleCount::ONE
    };
    return Texture::Create(createInfo, name);
  }
}