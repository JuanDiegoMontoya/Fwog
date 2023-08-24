#include "Fwog/detail/FramebufferCache.h"
#include "Fwog/Texture.h"
#include "Fwog/detail/ContextState.h"
#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  uint32_t FramebufferCache::CreateOrGetCachedFramebuffer(const RenderInfo& renderInfo)
  {
    RenderAttachments attachments;
    for (const auto& colorAttachment : renderInfo.colorAttachments)
    {
      attachments.colorAttachments.emplace_back(TextureProxy{
        colorAttachment.texture.get().GetCreateInfo(),
        detail::GetHandle(colorAttachment.texture),
      });
    }
    if (renderInfo.depthAttachment)
    {
      attachments.depthAttachment.emplace(TextureProxy{
        renderInfo.depthAttachment->texture.get().GetCreateInfo(),
        detail::GetHandle(renderInfo.depthAttachment->texture),
      });
    }
    if (renderInfo.stencilAttachment)
    {
      attachments.stencilAttachment.emplace(TextureProxy{
        renderInfo.stencilAttachment->texture.get().GetCreateInfo(),
        detail::GetHandle(renderInfo.stencilAttachment->texture),
      });
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

    detail::InvokeVerboseMessageCallback("Created framebuffer with handle ", fbo);

    framebufferCacheKey_.emplace_back(std::move(attachments));
    return framebufferCacheValue_.emplace_back(fbo);
  }

  void FramebufferCache::Clear()
  {
    for (const auto& fbo : framebufferCacheValue_)
    {
      detail::InvokeVerboseMessageCallback("Destroyed framebuffer with handle ", fbo);
      glDeleteFramebuffers(1, &fbo);
    }

    framebufferCacheKey_.clear();
    framebufferCacheValue_.clear();
  }

  // Must be called when a texture is deleted, otherwise the cache becomes invalid.
  void FramebufferCache::RemoveTexture(const Texture& texture)
  {
    const TextureProxy texp = {texture.GetCreateInfo(), detail::GetHandle(texture)};

    for (size_t i = 0; i < framebufferCacheKey_.size(); i++)
    {
      const auto attachments = framebufferCacheKey_[i];

      for (const auto& ci : attachments.colorAttachments)
      {
        if (texp == ci)
        {
          framebufferCacheKey_.erase(framebufferCacheKey_.begin() + i);
          auto fboIt = framebufferCacheValue_.begin() + i;
          glDeleteFramebuffers(1, &*fboIt);
          framebufferCacheValue_.erase(fboIt);
          i--;
          break;
        }
      }
      
      if (texp == attachments.depthAttachment || texp == attachments.stencilAttachment)
      {
        framebufferCacheKey_.erase(framebufferCacheKey_.begin() + i);
        auto fboIt = framebufferCacheValue_.begin() + i;
        glDeleteFramebuffers(1, &*fboIt);
        framebufferCacheValue_.erase(fboIt);
        i--;
        continue;
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

    // Nullity of the attachments differ
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
