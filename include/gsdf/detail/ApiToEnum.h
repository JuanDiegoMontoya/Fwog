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
}