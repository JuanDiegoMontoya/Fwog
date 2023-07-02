#include <Fwog/Texture.h>
#include <Fwog/detail/ApiToEnum.h>
#include <Fwog/detail/ContextState.h>

#include <array>
#include <new>
#include <utility>

#include FWOG_OPENGL_HEADER

#define MAX_NAME_LEN 256

namespace Fwog
{
  namespace detail
  {
    uint32_t GetHandle(const Texture& texture)
    {
      return const_cast<Texture&>(texture).Handle();
    }

    uint64_t GetBlockCompressedImageSize(Format format, uint32_t width, uint32_t height, uint32_t depth)
    {
      FWOG_ASSERT(detail::IsBlockCompressedFormat(format));

      // BCn formats store 4x4 blocks of pixels, even if the dimensions aren't a multiple of 4
      // We round up to the nearest multiple of 4 for width and height, but not depth, since
      // 3D BCn images are just multiple 2D images stacked
      width = (width + 4 - 1) & -4;
      height = (height + 4 - 1) & -4;

      switch (format)
      {
      // BC1 and BC4 store 4x4 blocks with 64 bits (8 bytes)
      case Format::BC1_RGB_UNORM:
      case Format::BC1_RGBA_UNORM:
      case Format::BC1_RGB_SRGB:
      case Format::BC1_RGBA_SRGB:
      case Format::BC4_R_UNORM:
      case Format::BC4_R_SNORM:
        return width * height * depth / 2;

      // BC3, BC5, BC6, and BC7 store 4x4 blocks with 128 bits (16 bytes)
      case Format::BC2_RGBA_UNORM:
      case Format::BC2_RGBA_SRGB:
      case Format::BC3_RGBA_UNORM:
      case Format::BC3_RGBA_SRGB:
      case Format::BC5_RG_UNORM:
      case Format::BC5_RG_SNORM:
      case Format::BC6H_RGB_UFLOAT:
      case Format::BC6H_RGB_SFLOAT:
      case Format::BC7_RGBA_UNORM:
      case Format::BC7_RGBA_SRGB:
        return width * height * depth;
      default: FWOG_UNREACHABLE; return 0;
      }
    }
  } // namespace detail

  Texture::Texture(const TextureCreateInfo& createInfo, std::string_view name) : createInfo_(createInfo)
  {
    glCreateTextures(detail::ImageTypeToGL(createInfo.imageType), 1, &id_);

    switch (createInfo.imageType)
    {
    case ImageType::TEX_1D:
      glTextureStorage1D(id_, createInfo.mipLevels, detail::FormatToGL(createInfo.format), createInfo.extent.width);
      break;
    case ImageType::TEX_2D:
      glTextureStorage2D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.extent.height);
      break;
    case ImageType::TEX_3D:
      glTextureStorage3D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.extent.height,
                         createInfo.extent.depth);
      break;
    case ImageType::TEX_1D_ARRAY:
      glTextureStorage2D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.arrayLayers);
      break;
    case ImageType::TEX_2D_ARRAY:
      glTextureStorage3D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.extent.height,
                         createInfo.arrayLayers);
      break;
    case ImageType::TEX_CUBEMAP:
      glTextureStorage2D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.extent.height);
      break;
    case ImageType::TEX_CUBEMAP_ARRAY:
      glTextureStorage3D(id_,
                         createInfo.mipLevels,
                         detail::FormatToGL(createInfo.format),
                         createInfo.extent.width,
                         createInfo.extent.height,
                         createInfo.arrayLayers);
      break;
    case ImageType::TEX_2D_MULTISAMPLE:
      glTextureStorage2DMultisample(id_,
                                    detail::SampleCountToGL(createInfo.sampleCount),
                                    detail::FormatToGL(createInfo.format),
                                    createInfo.extent.width,
                                    createInfo.extent.height,
                                    GL_TRUE);
      break;
    case ImageType::TEX_2D_MULTISAMPLE_ARRAY:
      glTextureStorage3DMultisample(id_,
                                    detail::SampleCountToGL(createInfo.sampleCount),
                                    detail::FormatToGL(createInfo.format),
                                    createInfo.extent.width,
                                    createInfo.extent.height,
                                    createInfo.arrayLayers,
                                    GL_TRUE);
      break;
    default: FWOG_UNREACHABLE; break;
    }

    if (!name.empty())
    {
      glObjectLabel(GL_TEXTURE, id_, static_cast<GLsizei>(name.length()), name.data());
    }

    detail::InvokeVerboseMessageCallback("Created texture with handle {}", id_);
  }

  Texture::Texture(Texture&& old) noexcept
    : id_(std::exchange(old.id_, 0)), createInfo_(old.createInfo_), bindlessHandle_(std::exchange(old.bindlessHandle_, 0))
  {
  }

  Texture& Texture::operator=(Texture&& old) noexcept
  {
    if (&old == this)
      return *this;
    this->~Texture();
    return *new (this) Texture(std::move(old));
  }

  Texture::~Texture()
  {
    if (id_ == 0)
    {
      return;
    }

    if (bindlessHandle_ != 0)
    {
      glMakeTextureHandleNonResidentARB(bindlessHandle_);
    }

    detail::InvokeVerboseMessageCallback("Destroyed texture with handle {}", id_);
    glDeleteTextures(1, &id_);
    // Ensure that the texture is no longer referenced in the FBO cache
    Fwog::detail::context->fboCache.RemoveTexture(*this);
  }

  TextureView Texture::CreateSingleMipView(uint32_t level)
  {
    TextureViewCreateInfo createInfo{
      .viewType = createInfo_.imageType,
      .format = createInfo_.format,
      .minLevel = level,
      .numLevels = 1,
      .minLayer = 0,
      .numLayers = createInfo_.arrayLayers,
    };
    return TextureView(createInfo, *this);
  }

  TextureView Texture::CreateSingleLayerView(uint32_t layer)
  {
    TextureViewCreateInfo createInfo{
      .viewType = createInfo_.imageType,
      .format = createInfo_.format,
      .minLevel = 0,
      .numLevels = createInfo_.mipLevels,
      .minLayer = layer,
      .numLayers = 1,
    };
    return TextureView(createInfo, *this);
  }

  TextureView Texture::CreateFormatView(Format newFormat)
  {
    TextureViewCreateInfo createInfo{
      .viewType = createInfo_.imageType,
      .format = newFormat,
      .minLevel = 0,
      .numLevels = createInfo_.mipLevels,
      .minLayer = 0,
      .numLayers = createInfo_.arrayLayers,
    };
    return TextureView(createInfo, *this);
  }

  TextureView Texture::CreateSwizzleView(ComponentMapping components)
  {
    TextureViewCreateInfo createInfo{
      .viewType = createInfo_.imageType,
      .format = createInfo_.format,
      .components = components,
      .minLevel = 0,
      .numLevels = createInfo_.mipLevels,
      .minLayer = 0,
      .numLayers = createInfo_.arrayLayers,
    };
    return TextureView(createInfo, *this);
  }

  uint64_t Texture::GetBindlessHandle(Sampler sampler)
  {
    FWOG_ASSERT(detail::context->properties.features.bindlessTextures && "GL_ARB_bindless_texture is not supported");
    FWOG_ASSERT(bindlessHandle_ == 0 && "Texture already has bindless handle resident.");
    bindlessHandle_ = glGetTextureSamplerHandleARB(id_, sampler.Handle());
    FWOG_ASSERT(bindlessHandle_ != 0 && "Failed to create texture sampler handle.");
    glMakeTextureHandleResidentARB(bindlessHandle_);
    return bindlessHandle_;
  }

  void Texture::UpdateImage(const TextureUpdateInfo& info)
  {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    subImageInternal(info);
  }

  void Texture::UpdateCompressedImage(const CompressedTextureUpdateInfo& info)
  {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    subCompressedImageInternal(info);
  }

  void Texture::subImageInternal(const TextureUpdateInfo& info)
  {
    FWOG_ASSERT(!detail::IsBlockCompressedFormat(createInfo_.format));
    GLenum format{};
    if (info.format == UploadFormat::INFER_FORMAT)
    {
      format = detail::UploadFormatToGL(detail::FormatToUploadFormat(createInfo_.format));
    }
    else
    {
      format = detail::UploadFormatToGL(info.format);
    }

    GLenum type{};
    if (info.type == UploadType::INFER_TYPE)
    {
      type = detail::FormatToTypeGL(createInfo_.format);
    }
    else
    {
      type = detail::UploadTypeToGL(info.type);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, info.rowLength);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, info.imageHeight);

    switch (detail::ImageTypeToDimension(createInfo_.imageType))
    {
    case 1:
      glTextureSubImage1D(id_, info.level, info.offset.x, info.extent.width, format, type, info.pixels); break;
    case 2:
      glTextureSubImage2D(id_,
                          info.level,
                          info.offset.x,
                          info.offset.y,
                          info.extent.width,
                          info.extent.height,
                          format,
                          type,
                          info.pixels);
      break;
    case 3:
      glTextureSubImage3D(id_,
                          info.level,
                          info.offset.x,
                          info.offset.y,
                          info.offset.z,
                          info.extent.width,
                          info.extent.height,
                          info.extent.depth,
                          format,
                          type,
                          info.pixels);
      break;
    }
  }

  void Texture::subCompressedImageInternal(const CompressedTextureUpdateInfo& info)
  {
    FWOG_ASSERT(detail::IsBlockCompressedFormat(createInfo_.format));
    const GLenum format = detail::FormatToGL(createInfo_.format);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

    switch (detail::ImageTypeToDimension(createInfo_.imageType))
    {
    case 2:
      glCompressedTextureSubImage2D(
        id_,
        info.level,
        info.offset.x,
        info.offset.y,
        info.extent.width,
        info.extent.height,
        format,
        static_cast<uint32_t>(detail::GetBlockCompressedImageSize(createInfo_.format, info.extent.width, info.extent.height, 1)),
        info.data);
      break;
    case 3:
      glCompressedTextureSubImage3D(
        id_,
        info.level,
        info.offset.x,
        info.offset.y,
        info.offset.z,
        info.extent.width,
        info.extent.height,
        info.extent.depth,
        format,
        static_cast<uint32_t>(detail::GetBlockCompressedImageSize(createInfo_.format, info.extent.width, info.extent.height, info.extent.depth)),
        info.data);
      break;
    default: FWOG_UNREACHABLE;
    }
  }

  void Texture::ClearImage(const TextureClearInfo& info)
  {
    // Infer format
    GLenum format{};
    if (info.format == UploadFormat::INFER_FORMAT)
    {
      format = detail::UploadFormatToGL(detail::FormatToUploadFormat(createInfo_.format));
    }
    else
    {
      format = detail::UploadFormatToGL(info.format);
    }

    // Infer type
    GLenum type{};
    if (info.type == UploadType::INFER_TYPE)
    {
      type = detail::FormatToTypeGL(createInfo_.format);
    }
    else
    {
      type = detail::UploadTypeToGL(info.type);
    }

    // Infer extent
    Extent3D extent = info.extent;
    if (extent == Extent3D{})
    {
      extent = createInfo_.extent;
    }

    glClearTexSubImage(id_,
                       info.level,
                       info.offset.x,
                       info.offset.y,
                       info.offset.z,
                       extent.width,
                       extent.height,
                       extent.depth,
                       format,
                       type,
                       info.data);
  }

  void Texture::GenMipmaps()
  {
    glGenerateTextureMipmap(id_);
  }

  TextureView::TextureView() {}

  TextureView::TextureView(const TextureViewCreateInfo& viewInfo, Texture& texture, std::string_view name)
    : viewInfo_(viewInfo)
  {
    createInfo_ = texture.GetCreateInfo();
    glGenTextures(1, &id_); // glCreateTextures does not work here
    glTextureView(id_,
                  detail::ImageTypeToGL(viewInfo.viewType),
                  texture.Handle(),
                  detail::FormatToGL(viewInfo.format),
                  viewInfo.minLevel,
                  viewInfo.numLevels,
                  viewInfo.minLayer,
                  viewInfo.numLayers);

    glTextureParameteri(id_, GL_TEXTURE_SWIZZLE_R, detail::ComponentSwizzleToGL(viewInfo.components.r));
    glTextureParameteri(id_, GL_TEXTURE_SWIZZLE_G, detail::ComponentSwizzleToGL(viewInfo.components.g));
    glTextureParameteri(id_, GL_TEXTURE_SWIZZLE_B, detail::ComponentSwizzleToGL(viewInfo.components.b));
    glTextureParameteri(id_, GL_TEXTURE_SWIZZLE_A, detail::ComponentSwizzleToGL(viewInfo.components.a));

    if (!name.empty())
    {
      glObjectLabel(GL_TEXTURE, id_, static_cast<GLsizei>(name.length()), name.data());
    }

    detail::InvokeVerboseMessageCallback("Created texture view with handle {}", id_);
  }

  TextureView::TextureView(const TextureViewCreateInfo& viewInfo, TextureView& textureView, std::string_view name)
    : TextureView(viewInfo, static_cast<Texture&>(textureView), name)
  {
    createInfo_ = TextureCreateInfo{
      .imageType = textureView.viewInfo_.viewType,
      .format = textureView.viewInfo_.format,
      .extent = textureView.createInfo_.extent,
      .mipLevels = textureView.viewInfo_.numLevels,
      .arrayLayers = textureView.viewInfo_.numLayers,
    };
  }

  TextureView::TextureView(Texture& texture, std::string_view name)
    : TextureView(
        TextureViewCreateInfo{
          .viewType = texture.GetCreateInfo().imageType,
          .format = texture.GetCreateInfo().format,
          .minLevel = 0,
          .numLevels = texture.GetCreateInfo().mipLevels,
          .minLayer = 0,
          .numLayers = texture.GetCreateInfo().arrayLayers,
        },
        texture,
        name)
  {
  }

  TextureView::TextureView(TextureView&& old) noexcept : Texture(std::move(old)), viewInfo_(old.viewInfo_) {}

  TextureView& TextureView::operator=(TextureView&& old) noexcept
  {
    if (&old == this)
      return *this;
    this->~TextureView();
    return *new (this) TextureView(std::move(old));
  }

  TextureView::~TextureView() {}

  Sampler::Sampler(const SamplerState& samplerState)
    : Sampler(Fwog::detail::context->samplerCache.CreateOrGetCachedTextureSampler(samplerState))
  {
  }

  Texture CreateTexture2D(Extent2D size, Format format, std::string_view name)
  {
    TextureCreateInfo createInfo{
      .imageType = ImageType::TEX_2D,
      .format = format,
      .extent = {size.width, size.height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .sampleCount = SampleCount::SAMPLES_1,
    };
    return Texture(createInfo, name);
  }

  Texture CreateTexture2DMip(Extent2D size, Format format, uint32_t mipLevels, std::string_view name)
  {
    TextureCreateInfo createInfo{
      .imageType = ImageType::TEX_2D,
      .format = format,
      .extent = {size.width, size.height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .sampleCount = SampleCount::SAMPLES_1,
    };
    return Texture(createInfo, name);
  }
} // namespace Fwog