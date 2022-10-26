#include <Fwog/Common.h>
#include <Fwog/detail/ApiToEnum.h>

namespace Fwog::detail
{
  // clang-format off
  GLenum FilterToGL(Filter filter)
  {
    switch (filter)
    {
    case Filter::NEAREST: return GL_NEAREST;
    case Filter::LINEAR:  return GL_LINEAR;
    default: FWOG_UNREACHABLE; return 0;
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

  GLbitfield BufferStorageFlagsToGL(BufferStorageFlags flags)
  {
    GLbitfield ret = 0;
    ret |= flags & BufferStorageFlag::DYNAMIC_STORAGE ? GL_DYNAMIC_STORAGE_BIT : 0;
    ret |= flags & BufferStorageFlag::CLIENT_STORAGE ?  GL_CLIENT_STORAGE_BIT : 0;
    return ret;
  }

  GLbitfield BufferMapFlagsToGL(BufferMapFlags flags)
  {
    GLbitfield ret = 0;
    ret |= flags & BufferMapFlag::MAP_READ ? GL_MAP_READ_BIT : 0;
    ret |= flags & BufferMapFlag::MAP_WRITE ? GL_MAP_WRITE_BIT : 0;
    ret |= flags & BufferMapFlag::MAP_PERSISTENT ? GL_MAP_PERSISTENT_BIT : 0;
    ret |= flags & BufferMapFlag::MAP_COHERENT ? GL_MAP_COHERENT_BIT : 0;
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
    default: FWOG_UNREACHABLE; return 0;
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
    default: FWOG_UNREACHABLE; return 0;
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
    case UploadFormat::R_INTEGER:       return GL_RED_INTEGER;
    case UploadFormat::RG_INTEGER:      return GL_RG_INTEGER;
    case UploadFormat::RGB_INTEGER:     return GL_RGB_INTEGER;
    case UploadFormat::BGR_INTEGER:     return GL_BGR_INTEGER;
    case UploadFormat::RGBA_INTEGER:    return GL_RGBA_INTEGER;
    case UploadFormat::BGRA_INTEGER:    return GL_BGRA_INTEGER;
    case UploadFormat::DEPTH_COMPONENT: return GL_DEPTH_COMPONENT;
    case UploadFormat::STENCIL_INDEX:   return GL_STENCIL_INDEX;
    case UploadFormat::DEPTH_STENCIL:   return GL_DEPTH_STENCIL;
    default: FWOG_UNREACHABLE; return 0;
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
    case UploadType::UBYTE_2_3_3_REV:         return GL_UNSIGNED_BYTE_2_3_3_REV;
    case UploadType::USHORT_5_6_5:        return GL_UNSIGNED_SHORT_5_6_5;
    case UploadType::USHORT_5_6_5_REV:    return GL_UNSIGNED_SHORT_5_6_5_REV;
    case UploadType::USHORT_4_4_4_4:      return GL_UNSIGNED_SHORT_4_4_4_4;
    case UploadType::USHORT_4_4_4_4_REV:  return GL_UNSIGNED_SHORT_4_4_4_4_REV;
    case UploadType::USHORT_5_5_5_1:      return GL_UNSIGNED_SHORT_5_5_5_1;
    case UploadType::USHORT_1_5_5_5_REV:  return GL_UNSIGNED_SHORT_1_5_5_5_REV;
    case UploadType::UINT_8_8_8_8:        return GL_UNSIGNED_INT_8_8_8_8;
    case UploadType::UINT_8_8_8_8_REV:    return GL_UNSIGNED_INT_8_8_8_8_REV;
    case UploadType::UINT_10_10_10_2:     return GL_UNSIGNED_INT_10_10_10_2;
    case UploadType::UINT_2_10_10_10_REV: return GL_UNSIGNED_INT_2_10_10_10_REV;
    default: FWOG_UNREACHABLE; return 0;
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
    default: FWOG_UNREACHABLE; return 0;
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
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum CullModeToGL(CullMode mode)
  {
    switch (mode)
    {
    case CullMode::NONE: return 0;
    case CullMode::FRONT: return GL_FRONT;
    case CullMode::BACK: return GL_BACK;
    case CullMode::FRONT_AND_BACK: return GL_FRONT_AND_BACK;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum PolygonModeToGL(PolygonMode mode)
  {
    switch (mode)
    {
    case PolygonMode::FILL: return GL_FILL;
    case PolygonMode::LINE: return GL_LINE;
    case PolygonMode::POINT: return GL_POINT;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum FrontFaceToGL(FrontFace face)
  {
    switch (face)
    {
    case FrontFace::CLOCKWISE: return GL_CW;
    case FrontFace::COUNTERCLOCKWISE: return GL_CCW;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum LogicOpToGL(LogicOp op)
  {
    switch (op)
    {
    case LogicOp::CLEAR: return GL_CLEAR;
    case LogicOp::SET: return GL_SET;
    case LogicOp::COPY: return GL_COPY;
    case LogicOp::COPY_INVERTED: return GL_COPY_INVERTED;
    case LogicOp::NO_OP: return GL_NOOP;
    case LogicOp::INVERT: return GL_INVERT;
    case LogicOp::AND: return GL_AND;
    case LogicOp::NAND: return GL_NAND;
    case LogicOp::OR: return GL_OR;
    case LogicOp::NOR: return GL_NOR;
    case LogicOp::XOR: return GL_XOR;
    case LogicOp::EQUIVALENT: return GL_EQUIV;
    case LogicOp::AND_REVERSE: return GL_AND_REVERSE;
    case LogicOp::OR_REVERSE: return GL_OR_REVERSE;
    case LogicOp::AND_INVERTED: return GL_AND_INVERTED;
    case LogicOp::OR_INVERTED: return GL_OR_INVERTED;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum BlendFactorToGL(BlendFactor factor)
  {
    switch (factor)
    {
    case BlendFactor::ZERO: return GL_ZERO;
    case BlendFactor::ONE: return GL_ONE;
    case BlendFactor::SRC_COLOR: return GL_SRC_COLOR;
    case BlendFactor::ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DST_COLOR: return GL_DST_COLOR;
    case BlendFactor::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
    case BlendFactor::SRC_ALPHA: return GL_SRC_ALPHA;
    case BlendFactor::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DST_ALPHA: return GL_DST_ALPHA;
    case BlendFactor::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::CONSTANT_COLOR: return GL_CONSTANT_COLOR;
    case BlendFactor::ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
    case BlendFactor::CONSTANT_ALPHA: return GL_CONSTANT_ALPHA;
    case BlendFactor::ONE_MINUS_CONSTANT_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
    case BlendFactor::SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
    case BlendFactor::SRC1_COLOR: return GL_SRC1_COLOR;
    case BlendFactor::ONE_MINUS_SRC1_COLOR: return GL_ONE_MINUS_SRC1_COLOR;
    case BlendFactor::SRC1_ALPHA: return GL_SRC1_ALPHA;
    case BlendFactor::ONE_MINUS_SRC1_ALPHA: return GL_ONE_MINUS_SRC1_ALPHA;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum BlendOpToGL(BlendOp op)
  {
    switch (op)
    {
    case BlendOp::ADD: return GL_FUNC_ADD;
    case BlendOp::SUBTRACT: return GL_FUNC_SUBTRACT;
    case BlendOp::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case BlendOp::MIN: return GL_MIN;
    case BlendOp::MAX: return GL_MAX;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum FormatToTypeGL(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:
    case Format::R8G8_UNORM:
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8A8_UNORM:
    case Format::R8_UINT:
    case Format::R8G8_UINT:
    case Format::R8G8B8_UINT:
    case Format::R8G8B8A8_UINT:
      return GL_UNSIGNED_BYTE;
    case Format::R8_SNORM:
    case Format::R8G8_SNORM:
    case Format::R8G8B8_SNORM:
    case Format::R8G8B8A8_SNORM:
    case Format::R8_SINT:
    case Format::R8G8_SINT:
    case Format::R8G8B8_SINT:
    case Format::R8G8B8A8_SINT:
      return GL_BYTE;
    case Format::R16_UNORM:
    case Format::R16G16_UNORM:
    case Format::R16G16B16A16_UNORM:
    case Format::R16_UINT:
    case Format::R16G16_UINT:
    case Format::R16G16B16_UINT:
    case Format::R16G16B16A16_UINT:
      return GL_UNSIGNED_SHORT;
    case Format::R16_SNORM:
    case Format::R16G16_SNORM:
    case Format::R16G16B16_SNORM:
    case Format::R16_SINT:
    case Format::R16G16_SINT:
    case Format::R16G16B16_SINT:
    case Format::R16G16B16A16_SINT:
      return GL_SHORT;
    case Format::R16_FLOAT:
    case Format::R16G16_FLOAT:
    case Format::R16G16B16_FLOAT:
    case Format::R16G16B16A16_FLOAT:
      return GL_HALF_FLOAT;
    case Format::R32_FLOAT:
    case Format::R32G32_FLOAT:
    case Format::R32G32B32_FLOAT:
    case Format::R32G32B32A32_FLOAT:
      return GL_FLOAT;
    case Format::R32_SINT:
    case Format::R32G32_SINT:
    case Format::R32G32B32_SINT:
    case Format::R32G32B32A32_SINT:
      return GL_INT;
    case Format::R32_UINT:
    case Format::R32G32_UINT:
    case Format::R32G32B32_UINT:
    case Format::R32G32B32A32_UINT:
      return GL_UNSIGNED_INT;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLint FormatToSizeGL(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:
    case Format::R8_SNORM:
    case Format::R16_UNORM:
    case Format::R16_SNORM:
    case Format::R16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R8_SINT:
    case Format::R16_SINT:
    case Format::R32_SINT:
    case Format::R8_UINT:
    case Format::R16_UINT:
    case Format::R32_UINT:
      return 1;
    case Format::R8G8_UNORM:
    case Format::R8G8_SNORM:
    case Format::R16G16_FLOAT:
    case Format::R16G16_UNORM:
    case Format::R16G16_SNORM:
    case Format::R32G32_FLOAT:
    case Format::R8G8_SINT:
    case Format::R16G16_SINT:
    case Format::R32G32_SINT:
    case Format::R8G8_UINT:
    case Format::R16G16_UINT:
    case Format::R32G32_UINT:
      return 2;
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8_SNORM:
    case Format::R16G16B16_SNORM:
    case Format::R16G16B16_FLOAT:
    case Format::R32G32B32_FLOAT:
    case Format::R8G8B8_SINT:
    case Format::R16G16B16_SINT:
    case Format::R32G32B32_SINT:
    case Format::R8G8B8_UINT:
    case Format::R16G16B16_UINT:
    case Format::R32G32B32_UINT:
      return 3;
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_SNORM:
    case Format::R16G16B16A16_UNORM:
    case Format::R16G16B16A16_FLOAT:
    case Format::R32G32B32A32_FLOAT:
    case Format::R8G8B8A8_SINT:
    case Format::R16G16B16A16_SINT:
    case Format::R32G32B32A32_SINT:
    case Format::R10G10B10A2_UINT:
    case Format::R8G8B8A8_UINT:
    case Format::R16G16B16A16_UINT:
    case Format::R32G32B32A32_UINT:
      return 4;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLboolean IsFormatNormalizedGL(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:
    case Format::R8_SNORM:
    case Format::R16_UNORM:
    case Format::R16_SNORM:
    case Format::R8G8_UNORM:
    case Format::R8G8_SNORM:
    case Format::R16G16_UNORM:
    case Format::R16G16_SNORM:
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8_SNORM:
    case Format::R16G16B16_SNORM:
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_SNORM:
    case Format::R16G16B16A16_UNORM:
      return GL_TRUE;
    case Format::R16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R8_SINT:
    case Format::R16_SINT:
    case Format::R32_SINT:
    case Format::R8_UINT:
    case Format::R16_UINT:
    case Format::R32_UINT:
    case Format::R16G16_FLOAT:
    case Format::R32G32_FLOAT:
    case Format::R8G8_SINT:
    case Format::R16G16_SINT:
    case Format::R32G32_SINT:
    case Format::R8G8_UINT:
    case Format::R16G16_UINT:
    case Format::R32G32_UINT:
    case Format::R16G16B16_FLOAT:
    case Format::R32G32B32_FLOAT:
    case Format::R8G8B8_SINT:
    case Format::R16G16B16_SINT:
    case Format::R32G32B32_SINT:
    case Format::R8G8B8_UINT:
    case Format::R16G16B16_UINT:
    case Format::R32G32B32_UINT:
    case Format::R16G16B16A16_FLOAT:
    case Format::R32G32B32A32_FLOAT:
    case Format::R8G8B8A8_SINT:
    case Format::R16G16B16A16_SINT:
    case Format::R32G32B32A32_SINT:
    case Format::R10G10B10A2_UINT:
    case Format::R8G8B8A8_UINT:
    case Format::R16G16B16A16_UINT:
    case Format::R32G32B32A32_UINT:
      return GL_FALSE;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GlFormatClass FormatToFormatClass(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:
    case Format::R8_SNORM:
    case Format::R16_UNORM:
    case Format::R16_SNORM:
    case Format::R8G8_UNORM:
    case Format::R8G8_SNORM:
    case Format::R16G16_UNORM:
    case Format::R16G16_SNORM:
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8_SNORM:
    case Format::R16G16B16_SNORM:
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_SNORM:
    case Format::R16G16B16A16_UNORM:
    case Format::R16_FLOAT:
    case Format::R16G16_FLOAT:
    case Format::R16G16B16_FLOAT:
    case Format::R16G16B16A16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R32G32_FLOAT:
    case Format::R32G32B32_FLOAT:
    case Format::R32G32B32A32_FLOAT:
      return GlFormatClass::FLOAT;
    case Format::R8_SINT:
    case Format::R16_SINT:
    case Format::R32_SINT:
    case Format::R8G8_SINT:
    case Format::R16G16_SINT:
    case Format::R32G32_SINT:
    case Format::R8G8B8_SINT:
    case Format::R16G16B16_SINT:
    case Format::R32G32B32_SINT:
    case Format::R8G8B8A8_SINT:
    case Format::R16G16B16A16_SINT:
    case Format::R32G32B32A32_SINT:
    case Format::R10G10B10A2_UINT:
    case Format::R8_UINT:
    case Format::R16_UINT:
    case Format::R32_UINT:
    case Format::R8G8_UINT:
    case Format::R16G16_UINT:
    case Format::R32G32_UINT:
    case Format::R8G8B8_UINT:
    case Format::R16G16B16_UINT:
    case Format::R32G32B32_UINT:
    case Format::R8G8B8A8_UINT:
    case Format::R16G16B16A16_UINT:
    case Format::R32G32B32A32_UINT:
      return GlFormatClass::INT;
    default: FWOG_UNREACHABLE; return GlFormatClass::LONG;
    }
  }

  GlBaseTypeClass FormatToBaseTypeClass(Format format)
  {
    switch (format)
    {
    case Format::R8_UNORM:
    case Format::R8_SNORM:
    case Format::R16_UNORM:
    case Format::R16_SNORM:
    case Format::R8G8_UNORM:
    case Format::R8G8_SNORM:
    case Format::R16G16_UNORM:
    case Format::R16G16_SNORM:
    case Format::R3G3B2_UNORM:
    case Format::R4G4B4_UNORM:
    case Format::R5G5B5_UNORM:
    case Format::R8G8B8_UNORM:
    case Format::R8G8B8_SNORM:
    case Format::R10G10B10_UNORM:
    case Format::R12G12B12_UNORM:
    case Format::R16G16B16_SNORM:
    case Format::R2G2B2A2_UNORM:
    case Format::R4G4B4A4_UNORM:
    case Format::R5G5B5A1_UNORM:
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_SNORM:
    case Format::R10G10B10A2_UNORM:
    case Format::R12G12B12A12_UNORM:
    case Format::R16G16B16A16_UNORM:
    case Format::R8G8B8_SRGB:
    case Format::R8G8B8A8_SRGB:
    case Format::R16_FLOAT:
    case Format::R16G16_FLOAT:
    case Format::R16G16B16_FLOAT:
    case Format::R16G16B16A16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R32G32_FLOAT:
    case Format::R32G32B32_FLOAT:
    case Format::R32G32B32A32_FLOAT:
    case Format::R11G11B10_FLOAT:
    case Format::R9G9B9_E5:
      return GlBaseTypeClass::FLOAT;
    case Format::R8_SINT:
    case Format::R16_SINT:
    case Format::R32_SINT:
    case Format::R8G8_SINT:
    case Format::R16G16_SINT:
    case Format::R32G32_SINT:
    case Format::R8G8B8_SINT:
    case Format::R16G16B16_SINT:
    case Format::R32G32B32_SINT:
    case Format::R8G8B8A8_SINT:
    case Format::R16G16B16A16_SINT:
    case Format::R32G32B32A32_SINT:
      return GlBaseTypeClass::SINT;
    case Format::R10G10B10A2_UINT:
    case Format::R8_UINT:
    case Format::R16_UINT:
    case Format::R32_UINT:
    case Format::R8G8_UINT:
    case Format::R16G16_UINT:
    case Format::R32G32_UINT:
    case Format::R8G8B8_UINT:
    case Format::R16G16B16_UINT:
    case Format::R32G32B32_UINT:
    case Format::R8G8B8A8_UINT:
    case Format::R16G16B16A16_UINT:
    case Format::R32G32B32A32_UINT:
      return GlBaseTypeClass::UINT;
    default: FWOG_UNREACHABLE; return GlBaseTypeClass::FLOAT;
    }
  }

  GLenum PrimitiveTopologyToGL(PrimitiveTopology topology)
  {
    switch (topology)
    {
    case PrimitiveTopology::POINT_LIST: return GL_POINTS;
    case PrimitiveTopology::LINE_LIST: return GL_LINES;
    case PrimitiveTopology::LINE_STRIP: return GL_LINE_STRIP;
    case PrimitiveTopology::TRIANGLE_LIST: return GL_TRIANGLES;
    case PrimitiveTopology::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case PrimitiveTopology::TRIANGLE_FAN: return GL_TRIANGLE_FAN;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum IndexTypeToGL(IndexType type)
  {
    switch (type)
    {
    case IndexType::UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
    case IndexType::UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
    case IndexType::UNSIGNED_INT: return GL_UNSIGNED_INT;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum CompareOpToGL(CompareOp op)
  {
    switch (op)
    {
    case CompareOp::NEVER: return GL_NEVER;
    case CompareOp::LESS: return GL_LESS;
    case CompareOp::EQUAL: return GL_EQUAL;
    case CompareOp::LESS_OR_EQUAL: return GL_LEQUAL;
    case CompareOp::GREATER: return GL_GREATER;
    case CompareOp::NOT_EQUAL: return GL_NOTEQUAL;
    case CompareOp::GREATER_OR_EQUAL: return GL_GEQUAL;
    case CompareOp::ALWAYS: return GL_ALWAYS;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLenum StencilOpToGL(StencilOp op)
  {
    switch (op)
    {
    case StencilOp::KEEP: return GL_KEEP;
    case StencilOp::ZERO: return GL_ZERO;
    case StencilOp::REPLACE: return GL_REPLACE;
    case StencilOp::INCREMENT_AND_CLAMP: return GL_INCR;
    case StencilOp::DECREMENT_AND_CLAMP: return GL_DECR;
    case StencilOp::INVERT: return GL_INVERT;
    case StencilOp::INCREMENT_AND_WRAP: return GL_INCR_WRAP;
    case StencilOp::DECREMENT_AND_WRAP: return GL_DECR_WRAP;
    default: FWOG_UNREACHABLE; return 0;
    }
  }

  GLbitfield BarrierBitsToGL(MemoryBarrierAccessBits bits)
  {
    GLbitfield ret = 0;
    ret |= bits & MemoryBarrierAccessBit::VERTEX_BUFFER_BIT ? GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::INDEX_BUFFER_BIT ? GL_ELEMENT_ARRAY_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::UNIFORM_BUFFER_BIT ? GL_UNIFORM_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::TEXTURE_FETCH_BIT ? GL_TEXTURE_FETCH_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::IMAGE_ACCESS_BIT ? GL_SHADER_IMAGE_ACCESS_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::COMMAND_BUFFER_BIT ? GL_COMMAND_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::TEXTURE_UPDATE_BIT ? GL_TEXTURE_UPDATE_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::BUFFER_UPDATE_BIT ? GL_BUFFER_UPDATE_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::MAPPED_BUFFER_BIT ? GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::FRAMEBUFFER_BIT ? GL_FRAMEBUFFER_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::SHADER_STORAGE_BIT ? GL_SHADER_STORAGE_BARRIER_BIT : 0;
    ret |= bits & MemoryBarrierAccessBit::QUERY_COUNTER_BIT ? GL_QUERY_BUFFER_BARRIER_BIT : 0;
    return ret;
  }
  // clang-format on
} // namespace Fwog::detail