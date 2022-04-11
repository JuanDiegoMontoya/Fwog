#pragma once
#include <vector>
#include <unordered_map>

namespace GFX
{
  class TextureView;
}

namespace GFX::detail
{
  struct RenderAttachments
  {
    std::vector<const TextureView*> colorAttachments;
    const TextureView* depthAttachment;
    const TextureView* stencilAttachment;

    bool operator==(const RenderAttachments& rhs) const;
  };
}

namespace std
{
  template<>
  struct hash<GFX::detail::RenderAttachments>
  {
    std::size_t operator()(const GFX::detail::RenderAttachments& k) const;
  };
}

namespace GFX::detail
{
  class FramebufferCache
  {
  public:
    uint32_t CreateOrGetCachedFramebuffer(const RenderAttachments& attachments);
    size_t Size() const { return framebufferCache_.size(); }
    void Clear();

  private:
    std::unordered_map<RenderAttachments, uint32_t> framebufferCache_;
  };
}