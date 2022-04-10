#include "gsdf/detail/FramebufferCache.h"
#include "gsdf/detail/Hash.h"
#include "gsdf/Texture.h"
#include "glad/gl.h"

namespace GFX::detail
{
  uint32_t FramebufferCache::CreateOrGetCachedFramebuffer(const RenderAttachments& attachments)
  {
    if (auto it = framebufferCache_.find(attachments); it != framebufferCache_.end())
    {
      return it->second;
    }

    uint32_t fbo{};
    glCreateFramebuffers(1, &fbo);
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < attachments.colorAttachments.size(); i++)
    {
      const auto& attachment = attachments.colorAttachments[i];
      glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, attachment->Handle(), 0);
      drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glNamedFramebufferDrawBuffers(fbo, drawBuffers.size(), drawBuffers.data());

    if (attachments.depthAttachment && attachments.stencilAttachment && attachments.depthAttachment == attachments.stencilAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, attachments.depthAttachment->Handle(), 0);
    }
    else if (attachments.depthAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, attachments.depthAttachment->Handle(), 0);
    }
    else if (attachments.stencilAttachment)
    {
      glNamedFramebufferTexture(fbo, GL_STENCIL_ATTACHMENT, attachments.stencilAttachment->Handle(), 0);
    }
    return framebufferCache_.insert({ attachments, fbo }).second;
  }

  void FramebufferCache::Clear()
  {
    framebufferCache_.clear();
  }

  bool RenderAttachments::operator==(const RenderAttachments& rhs) const
  {
    if (colorAttachments.size() != rhs.colorAttachments.size())
      return false;
    for (size_t i = 0; i < colorAttachments.size(); i++)
    {
      if (colorAttachments[i] != rhs.colorAttachments[i])
        return false;
    }
    if (depthAttachment != rhs.depthAttachment)
      return false;
    if (stencilAttachment != rhs.stencilAttachment)
      return false;
    return true;
  }
}

std::size_t std::hash<GFX::detail::RenderAttachments>::operator()(const GFX::detail::RenderAttachments& k) const
{
  auto rtup = std::make_tuple(k.depthAttachment, k.stencilAttachment);

  auto hashVal = GFX::detail::hashing::hash<decltype(rtup)>{}(rtup);

  for (size_t i = 0; i < k.colorAttachments.size(); i++)
  {
    auto cctup = std::make_tuple(k.colorAttachments[i], i);
    auto chashVal = GFX::detail::hashing::hash<decltype(cctup)>{}(cctup);
    GFX::detail::hashing::hash_combine(hashVal, chashVal);
  }

  return hashVal;
}