#pragma once
#include "Fwog/Rendering.h"
#include "Fwog/Texture.h"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <optional>

namespace Fwog::detail
{
  struct TextureProxy
  {
    TextureCreateInfo createInfo;
    uint32_t id;

    bool operator==(const TextureProxy&) const noexcept = default;
  };

  struct RenderAttachments
  {
    std::vector<TextureProxy> colorAttachments{};
    std::optional<TextureProxy> depthAttachment{};
    std::optional<TextureProxy> stencilAttachment{};

    bool operator==(const RenderAttachments& rhs) const;
  };

  class FramebufferCache
  {
  public:
    uint32_t CreateOrGetCachedFramebuffer(const RenderInfo& renderInfo);
    std::size_t Size() const
    {
      return framebufferCacheKey_.size();
    }
    void Clear();

    void RemoveTexture(const Texture& texture);

  private:
    std::vector<RenderAttachments> framebufferCacheKey_;
    std::vector<uint32_t> framebufferCacheValue_;
  };
} // namespace Fwog::detail
