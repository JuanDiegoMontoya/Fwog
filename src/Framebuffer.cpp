#include <gsdf/Common.h>
#include <gsdf/detail/ApiToEnum.h>
#include <gsdf/Framebuffer.h>
#include <gsdf/Texture.h>
#include <utility>

namespace GFX
{
  Framebuffer::Framebuffer()
  {
    glCreateFramebuffers(1, &handle_);
  }

  std::optional<Framebuffer> Framebuffer::Create()
  {
    return Framebuffer();
  }

  Framebuffer::Framebuffer(Framebuffer&& old) noexcept
  {
    handle_ = std::exchange(old.handle_, 0);
  }

  Framebuffer& Framebuffer::operator=(Framebuffer&& old) noexcept
  {
    if (this == &old) return *this;
    this->~Framebuffer();
    handle_ = std::exchange(old.handle_, 0);
    return *this;
  }

  Framebuffer::~Framebuffer()
  {
    if (handle_ != 0)
    {
      glDeleteFramebuffers(1, &handle_);
    }
  }

  void Framebuffer::SetAttachment(Attachment slot, const TextureView& view, uint32_t level)
  {
    glNamedFramebufferTexture(handle_, detail::AttachmentToGL(slot), view.id_, level);
  }

  void Framebuffer::ResetAttachment(Attachment slot)
  {
    glNamedFramebufferTexture(handle_, detail::AttachmentToGL(slot), 0, 0);
  }

  void Framebuffer::SetDrawBuffers(std::span<const Attachment> slots)
  {
    GSDF_ASSERT(slots.size() < 8);
    GLenum buffers[8]{};
    for (size_t i = 0; i < slots.size(); i++)
    {
      GSDF_ASSERT(slots[i] <= Attachment::COLOR_ATTACHMENT_MAX);
      buffers[i] = detail::AttachmentToGL(slots[i]);
    }
    glNamedFramebufferDrawBuffers(handle_, static_cast<GLsizei>(slots.size()), buffers);
  }

  bool Framebuffer::IsValid() const
  {
    GLenum status = glCheckNamedFramebufferStatus(handle_, GL_FRAMEBUFFER);
    return status == GL_FRAMEBUFFER_COMPLETE;
  }

  void Framebuffer::Bind()
  {
    glBindFramebuffer(GL_FRAMEBUFFER, handle_);
  }

  uint32_t Framebuffer::GetAttachmentAPIHandle(Attachment slot) const
  {
    GLint params{};
    glGetNamedFramebufferAttachmentParameteriv(
      handle_,
      detail::AttachmentToGL(slot),
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
      &params);
    return static_cast<uint32_t>(params);
  }

  void Framebuffer::Blit(const Framebuffer& source, const Framebuffer& target,
    Rect2D sourceRect, Rect2D targetRect,
    AspectMask mask, Filter filter)
  {
    glBlitNamedFramebuffer(source.handle_, target.handle_,
      sourceRect.offset.x, sourceRect.offset.y, sourceRect.offset.x + sourceRect.extent.width, sourceRect.offset.y + sourceRect.extent.width,
      targetRect.offset.x, targetRect.offset.y, targetRect.offset.x + targetRect.extent.width, targetRect.offset.y + targetRect.extent.width,
      detail::AspectMaskToGL(mask), detail::FilterToGL(filter));
  }
}