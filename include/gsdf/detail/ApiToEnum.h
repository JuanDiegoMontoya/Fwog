#pragma once
#include <glad/gl.h>
#include <gsdf/BasicTypes.h>
#include <gsdf/Buffer.h>

namespace GFX::detail
{
  ////////////////////////////////////////////////////////// framebuffer
  GLenum AttachmentToGL(Attachment attachment);

  GLenum FilterToGL(Filter filter);

  GLbitfield AspectMaskToGL(AspectMask bits);

  ////////////////////////////////////////////////////////// buffer
  GLbitfield BufferFlagsToGL(BufferFlags flags);

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
  enum class GlFormatClass { FLOAT, SINT, UINT, LONG };
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
  GlFormatClass FormatToInternalType(Format format);
}