#pragma once
#include <glad/gl.h>
#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>

namespace Fwog::detail
{
  ////////////////////////////////////////////////////////// framebuffer
  GLenum FilterToGL(Filter filter);

  GLbitfield AspectMaskToGL(AspectMask bits);

  ////////////////////////////////////////////////////////// buffer
  GLbitfield BufferStorageFlagsToGL(BufferStorageFlags flags);
  GLbitfield BufferMapFlagsToGL(BufferMapFlags flags);

  ////////////////////////////////////////////////////////// texture
  GLint ImageTypeToGL(ImageType imageType);

  GLint FormatToGL(Format format);

  GLint UploadFormatToGL(UploadFormat uploadFormat);

  GLint UploadTypeToGL(UploadType uploadType);

  GLint AddressModeToGL(AddressMode addressMode);

  GLsizei SampleCountToGL(SampleCount sampleCount);

  ////////////////////////////////////////////////////////// pipeline
  GLenum CullModeToGL(CullMode mode);
  GLenum PolygonModeToGL(PolygonMode mode);
  GLenum FrontFaceToGL(FrontFace face);
  GLenum LogicOpToGL(LogicOp op);
  GLenum BlendFactorToGL(BlendFactor factor);
  GLenum BlendOpToGL(BlendOp op);
  
  // arguments for glVertexArrayAttrib*Format
  enum class GlFormatClass { FLOAT, INT, LONG };
  struct GlVertexFormat
  {
    GLenum type;          // GL_FLOAT, etc.
    GLint size;           // 1, 2, 3, 4
    GLboolean normalized; // GL_TRUE, GL_FALSE
    GlFormatClass formatClass; // whether to call Format, IFormat, or LFormat
  };
  GLenum FormatToTypeGL(Format format);
  GLint FormatToSizeGL(Format format);
  GLboolean IsFormatNormalizedGL(Format format);
  GlFormatClass FormatToFormatClass(Format format);

  // for clearing color textures, we need to know which of these the texture holds
  enum class GlBaseTypeClass { FLOAT, SINT, UINT };
  GlBaseTypeClass FormatToBaseTypeClass(Format format);


  ////////////////////////////////////////////////////////// drawing
  GLenum PrimitiveTopologyToGL(PrimitiveTopology topology);

  GLenum IndexTypeToGL(IndexType type);

  GLenum CompareOpToGL(CompareOp op);

  GLenum StencilOpToGL(StencilOp op);

  GLbitfield BarrierBitsToGL(MemoryBarrierAccessBits bits);
}
