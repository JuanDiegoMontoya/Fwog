#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Fwog
{
  class Texture;
}

namespace Fwog::detail
{
  struct RenderAttachments
  {
    std::vector<const Texture*> colorAttachments;
    const Texture* depthAttachment = nullptr;
    const Texture* stencilAttachment = nullptr;

    bool operator==(const RenderAttachments& rhs) const;
  };
}

namespace std
{
  template<>
  struct hash<Fwog::detail::RenderAttachments>
  {
    std::size_t operator()(const Fwog::detail::RenderAttachments& k) const;
  };
}

namespace Fwog::detail
{
  class FramebufferCache
  {
  public:
    uint32_t CreateOrGetCachedFramebuffer(const RenderAttachments& attachments);
    std::size_t Size() const { return framebufferCache_.size(); }
    void Clear();

  private:
    std::unordered_map<RenderAttachments, uint32_t> framebufferCache_;
  };
}
