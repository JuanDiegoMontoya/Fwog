#include <gsdf/Common.h>
#include <gsdf/detail/ApiToEnum.h>

namespace GFX::detail
{
  GLenum AttachmentToGL(Attachment attachment)
  {
    switch (attachment)
    {
    case Attachment::NONE:          return GL_NONE;
    case Attachment::COLOR_0:       return GL_COLOR_ATTACHMENT0;
    case Attachment::COLOR_1:       return GL_COLOR_ATTACHMENT1;
    case Attachment::COLOR_2:       return GL_COLOR_ATTACHMENT2;
    case Attachment::COLOR_3:       return GL_COLOR_ATTACHMENT3;
    case Attachment::DEPTH:         return GL_DEPTH_ATTACHMENT;
    case Attachment::STENCIL:       return GL_STENCIL_ATTACHMENT;
    case Attachment::DEPTH_STENCIL: return GL_DEPTH_STENCIL_ATTACHMENT;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLenum FilterToGL(Filter filter)
  {
    switch (filter)
    {
    case Filter::NEAREST: return GL_NEAREST;
    case Filter::LINEAR:  return GL_LINEAR;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLbitfield AspectMaskToGL(AspectMask bits)
  {
    GLbitfield ret = 0;
    ret |= bits & AspectMaskBit::COLOR_BUFFER_BIT ? GL_COLOR_BUFFER_BIT : 0;
    ret |= bits & AspectMaskBit::DEPTH_BUFFER_BIT ? GL_DEPTH_BUFFER_BIT : 0;
    ret |= bits & AspectMaskBit::STENCIL_BUFFER_BIT ? GL_STENCIL_BUFFER_BIT : 0;
    return ret;
  }

  GLbitfield BufferFlagsToGL(BufferFlags flags)
  {
    GLbitfield ret = 0;
    ret |= flags & BufferFlag::DYNAMIC_STORAGE ? GL_DYNAMIC_STORAGE_BIT : 0;
    ret |= flags & BufferFlag::CLIENT_STORAGE ?  GL_CLIENT_STORAGE_BIT : 0;
    ret |= flags & BufferFlag::MAP_READ ?        GL_MAP_READ_BIT : 0;
    ret |= flags & BufferFlag::MAP_WRITE ?       GL_MAP_WRITE_BIT : 0;
    ret |= flags & BufferFlag::MAP_PERSISTENT ?  GL_MAP_PERSISTENT_BIT : 0;
    ret |= flags & BufferFlag::MAP_COHERENT ?    GL_MAP_COHERENT_BIT : 0;
    return ret;
  }

  GLint ImageTypeToGL(ImageType imageType)
  {
    switch (imageType)
    {
    case ImageType::TEX_1D:                   return GL_TEXTURE_1D;
    case ImageType::TEX_2D:                   return GL_TEXTURE_2D;
    case ImageType::TEX_3D:                   return GL_TEXTURE_3D;
    case ImageType::TEX_1D_ARRAY:             return GL_TEXTURE_1D_ARRAY;
    case ImageType::TEX_2D_ARRAY:             return GL_TEXTURE_2D_ARRAY;
    case ImageType::TEX_CUBEMAP:              return GL_TEXTURE_CUBE_MAP;
    case ImageType::TEX_2D_MULTISAMPLE:       return GL_TEXTURE_2D_MULTISAMPLE;
    case ImageType::TEX_2D_MULTISAMPLE_ARRAY: return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLint FormatToGL(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:           return GL_R8;
    case Format::R8_SNORM:           return GL_R8_SNORM;
    case Format::R16_UNORM:          return GL_R16;
    case Format::R16_SNORM:          return GL_R16_SNORM;
    case Format::R8G8_UNORM:         return GL_RG8;
    case Format::R8G8_SNORM:         return GL_RG8_SNORM;
    case Format::R16G16_UNORM:       return GL_RG16;
    case Format::R16G16_SNORM:       return GL_RG16_SNORM;
    case Format::R3G3B2_UNORM:       return GL_R3_G3_B2;
    case Format::R4G4B4_UNORM:       return GL_RGB4;
    case Format::R5G5B5_UNORM:       return GL_RGB5;
    case Format::R8G8B8_UNORM:       return GL_RGB8;
    case Format::R8G8B8_SNORM:       return GL_RGB8_SNORM;
    case Format::R10G10B10_UNORM:    return GL_RGB10;
    case Format::R12G12B12_UNORM:    return GL_RGB12;
      // GL_RG16?
    case Format::R16G16B16_SNORM:    return GL_RGB16_SNORM;
    case Format::R2G2B2A2_UNORM:     return GL_RGBA2;
    case Format::R4G4B4A4_UNORM:     return GL_RGBA4;
    case Format::R5G5B5A1_UNORM:     return GL_RGB5_A1;
    case Format::R8G8B8A8_UNORM:     return GL_RGBA8;
    case Format::R8G8B8A8_SNORM:     return GL_RGBA8_SNORM;
    case Format::R10G10B10A2_UNORM:  return GL_RGB10_A2;
    case Format::R10G10B10A2_UINT:   return GL_RGB10_A2UI;
    case Format::R12G12B12A12_UNORM: return GL_RGBA12;
    case Format::R16G16B16A16_UNORM: return GL_RGBA16;
    case Format::R8G8B8_SRGB:        return GL_SRGB8;
    case Format::R8G8B8A8_SRGB:      return GL_SRGB8_ALPHA8;
    case Format::R16_FLOAT:          return GL_R16F;
    case Format::R16G16_FLOAT:       return GL_RG16F;
    case Format::R16G16B16_FLOAT:    return GL_RGB16F;
    case Format::R16G16B16A16_FLOAT: return GL_RGBA16F;
    case Format::R32_FLOAT:          return GL_R32F;
    case Format::R32G32_FLOAT:       return GL_RG32F;
    case Format::R32G32B32_FLOAT:    return GL_RGB32F;
    case Format::R32G32B32A32_FLOAT: return GL_RGBA32F;
    case Format::R11G11B10_FLOAT:    return GL_R11F_G11F_B10F;
    case Format::R9G9B9_E5:          return GL_RGB9_E5;
    case Format::R8_SINT:            return GL_R8I;
    case Format::R8_UINT:            return GL_R8UI;
    case Format::R16_SINT:           return GL_R16I;
    case Format::R16_UINT:           return GL_R16UI;
    case Format::R32_SINT:           return GL_R32I;
    case Format::R32_UINT:           return GL_R32UI;
    case Format::R8G8_SINT:          return GL_RG8I;
    case Format::R8G8_UINT:          return GL_RG8UI;
    case Format::R16G16_SINT:        return GL_RG16I;
    case Format::R16G16_UINT:        return GL_RG16UI;
    case Format::R32G32_SINT:        return GL_RG32I;
    case Format::R32G32_UINT:        return GL_RG32UI;
    case Format::R8G8B8_SINT:        return GL_RGB8I;
    case Format::R8G8B8_UINT:        return GL_RGB8UI;
    case Format::R16G16B16_SINT:     return GL_RGB16I;
    case Format::R16G16B16_UINT:     return GL_RGB16UI;
    case Format::R32G32B32_SINT:     return GL_RGB32I;
    case Format::R32G32B32_UINT:     return GL_RGB32UI;
    case Format::R8G8B8A8_SINT:      return GL_RGBA8I;
    case Format::R8G8B8A8_UINT:      return GL_RGBA8UI;
    case Format::R16G16B16A16_SINT:  return GL_RGBA16I;
    case Format::R16G16B16A16_UINT:  return GL_RGBA16UI;
    case Format::R32G32B32A32_SINT:  return GL_RGBA32I;
    case Format::R32G32B32A32_UINT:  return GL_RGBA32UI;
    case Format::D32_FLOAT:          return GL_DEPTH_COMPONENT32F;
    case Format::D32_UNORM:          return GL_DEPTH_COMPONENT32;
    case Format::D24_UNORM:          return GL_DEPTH_COMPONENT24;
    case Format::D16_UNORM:          return GL_DEPTH_COMPONENT16;
    case Format::D32_FLOAT_S8_UINT:  return GL_DEPTH32F_STENCIL8;
    case Format::D24_UNORM_S8_UINT:  return GL_DEPTH24_STENCIL8;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLint UploadFormatToGL(UploadFormat uploadFormat)
  {
    switch (uploadFormat)
    {
    case UploadFormat::R:               return GL_RED;
    case UploadFormat::RG:              return GL_RG;
    case UploadFormat::RGB:             return GL_RGB;
    case UploadFormat::BGR:             return GL_BGR;
    case UploadFormat::RGBA:            return GL_RGBA;
    case UploadFormat::BGRA:            return GL_BGRA;
    case UploadFormat::DEPTH_COMPONENT: return GL_DEPTH_COMPONENT;
    case UploadFormat::STENCIL_INDEX:   return GL_STENCIL_INDEX;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLint UploadTypeToGL(UploadType uploadType)
  {
    switch (uploadType)
    {
    case UploadType::UBYTE:               return GL_UNSIGNED_BYTE;
    case UploadType::SBYTE:               return GL_BYTE;
    case UploadType::USHORT:              return GL_UNSIGNED_SHORT;
    case UploadType::SSHORT:              return GL_SHORT;
    case UploadType::UINT:                return GL_UNSIGNED_INT;
    case UploadType::SINT:                return GL_INT;
    case UploadType::FLOAT:               return GL_FLOAT;
    case UploadType::UBYTE_3_3_2:         return GL_UNSIGNED_BYTE_3_3_2;
    case UploadType::UBYTE_2_3_3:         return GL_UNSIGNED_BYTE_2_3_3_REV;
    case UploadType::USHORT_5_6_5:        return GL_UNSIGNED_SHORT_5_6_5;
    case UploadType::USHORT_5_6_5_REV:    return GL_UNSIGNED_SHORT_5_6_5_REV;
    case UploadType::USHORT_4_4_4_4:      return GL_UNSIGNED_SHORT_4_4_4_4;
    case UploadType::USHORT_4_4_4_4_REV:  return GL_UNSIGNED_SHORT_4_4_4_4_REV;
    case UploadType::USHORT_5_5_5_1:      return GL_UNSIGNED_SHORT_5_5_5_1;
    case UploadType::USHORT_5_5_5_1_REV:  return GL_UNSIGNED_SHORT_1_5_5_5_REV;
    case UploadType::UINT_8_8_8_8:        return GL_UNSIGNED_INT_8_8_8_8;
    case UploadType::UINT_8_8_8_8_REV:    return GL_UNSIGNED_INT_8_8_8_8_REV;
    case UploadType::UINT_10_10_10_2:     return GL_UNSIGNED_INT_10_10_10_2;
    case UploadType::UINT_10_10_10_2_REV: return GL_UNSIGNED_INT_2_10_10_10_REV;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLint AddressModeToGL(AddressMode addressMode)
  {
    switch (addressMode)
    {
    case AddressMode::REPEAT:               return GL_REPEAT;
    case AddressMode::MIRRORED_REPEAT:      return GL_MIRRORED_REPEAT;
    case AddressMode::CLAMP_TO_EDGE:        return GL_CLAMP_TO_EDGE;
    case AddressMode::CLAMP_TO_BORDER:      return GL_CLAMP_TO_BORDER;
    case AddressMode::MIRROR_CLAMP_TO_EDGE: return GL_MIRROR_CLAMP_TO_EDGE;
    default: GSDF_UNREACHABLE; return 0;
    }
  }

  GLsizei SampleCountToGL(SampleCount sampleCount)
  {
    switch (sampleCount)
    {
    case SampleCount::SAMPLES_1: return 1;
    case SampleCount::SAMPLES_2: return 2;
    case SampleCount::SAMPLES_4: return 4;
    case SampleCount::SAMPLES_8: return 8;
    case SampleCount::SAMPLES_16: return 16;
    default: GSDF_UNREACHABLE; return 0;
    }
  }
}