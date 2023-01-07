#include "Fwog/detail/FramebufferCache.h"
#include "Fwog/Texture.h"
#include "Fwog/detail/Hash.h"
#include "Fwog/Common.h"

namespace Fwog::detail
{
  uint32_t FramebufferCache::CreateOrGetCachedFramebuffer(const RenderInfo& renderInfo)
  {
    RenderAttachments attachments;
    for (const auto& colorAttachment : renderInfo.colorAttachments)
    {
      attachments.colorAttachments.emplace_back(colorAttachment.texture->CreateInfo(), colorAttachment.texture->Handle());
    }
    if (renderInfo.depthAttachment)
    {
      attachments.depthAttachment.emplace(renderInfo.depthAttachment->texture->CreateInfo(),
                                          renderInfo.depthAttachment->texture->Handle());
    }
    if (renderInfo.stencilAttachment)
    {
      attachments.stencilAttachment.emplace(renderInfo.stencilAttachment->texture->CreateInfo(),
                                            renderInfo.stencilAttachment->texture->Handle());
    }

    for (size_t i = 0; i < framebufferCacheKey_.size(); i++)
    {
      if (framebufferCacheKey_[i] == attachments)
        return framebufferCacheValue_[i];
    }

    uint32_t fbo{};
    glCreateFramebuffers(1, &fbo);
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < attachments.colorAttachments.size(); i++)
    {
      const auto& attachment = attachments.colorAttachments[i];
      glNamedFramebufferTexture(fbo, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), attachment.id, 0);
      drawBuffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
    }
    glNamedFramebufferDrawBuffers(fbo, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

    if (attachments.depthAttachment && attachments.stencilAttachment &&
        attachments.depthAttachment == attachments.stencilAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, attachments.depthAttachment->id, 0);
    }
    else if (attachments.depthAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, attachments.depthAttachment->id, 0);
    }
    else if (attachments.stencilAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_STENCIL_ATTACHMENT, attachments.stencilAttachment->id, 0);
    }

    framebufferCacheKey_.emplace_back(std::move(attachments));
    return framebufferCacheValue_.emplace_back(fbo);
  }

  void FramebufferCache::Clear()
  {
    for (const auto& fbo : framebufferCacheValue_)
    {
      glDeleteFramebuffers(1, &fbo);
    }

    framebufferCacheKey_.clear();
    framebufferCacheValue_.clear();
  }

  // Must be called when a texture is deleted, otherwise the cache becomes invalid.
  void FramebufferCache::RemoveTexture(const Texture& texture)
  {
    const TextureProxy texp = {texture.CreateInfo(), texture.Handle()};

    for (size_t i = 0; i < framebufferCacheKey_.size(); i++)
    {
      const auto& attachments = framebufferCacheKey_[i];

      for (const auto& ci : attachments.colorAttachments)
      {
        if (texp == ci)
        {
          framebufferCacheKey_.erase(framebufferCacheKey_.begin() + i);
          framebufferCacheValue_.erase(framebufferCacheValue_.begin() + i);
          i--;
        }
      }

      if (texp == attachments.depthAttachment)
      {
        framebufferCacheKey_.erase(framebufferCacheKey_.begin() + i);
        framebufferCacheValue_.erase(framebufferCacheValue_.begin() + i);
        i--;
      }

      if (texp == attachments.stencilAttachment)
      {
        framebufferCacheKey_.erase(framebufferCacheKey_.begin() + i);
        framebufferCacheValue_.erase(framebufferCacheValue_.begin() + i);
        i--;
      }
    }
  }

  bool RenderAttachments::operator==(const RenderAttachments& rhs) const
  {
    if (colorAttachments.size() != rhs.colorAttachments.size())
      return false;

    // Crucially, two attachments with the same address are not necessarily the same.
    // The inverse is also true: two attachments with different addresses are not necessarily different.

    for (size_t i = 0; i < colorAttachments.size(); i++)
    {
      // Color attachments must be non-null
      if (colorAttachments[i] != rhs.colorAttachments[i])
        return false;
    }

    // Nullness of the attachments differ
    if ((depthAttachment && !rhs.depthAttachment) || (!depthAttachment && rhs.depthAttachment))
      return false;
    // Both attachments are non-null, but have different values
    if (depthAttachment && rhs.depthAttachment && (*depthAttachment != *rhs.depthAttachment))
      return false;

    if ((stencilAttachment && !rhs.stencilAttachment) || (!stencilAttachment && rhs.stencilAttachment))
      return false;
    if (stencilAttachment && rhs.stencilAttachment && (*stencilAttachment != *rhs.stencilAttachment))
      return false;

    return true;
  }
} // namespace Fwog::detail
