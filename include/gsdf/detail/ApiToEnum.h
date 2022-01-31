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
  GLenum BufferTargetToGL(BufferTarget bufferTarget);

  GLbitfield BufferFlagsToGL(BufferFlags flags);

  // texture
  GLint ImageTypeToGL(ImageType imageType);

  GLint FormatToGL(Format format);

  GLint UploadFormatToGL(UploadFormat uploadFormat);

  GLint UploadTypeToGL(UploadType uploadType);

  GLint AddressModeToGL(AddressMode addressMode);

  GLsizei SampleCountToGL(SampleCount sampleCount);
}