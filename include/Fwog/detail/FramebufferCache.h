#pragma once
#include "Fwog/Rendering.h"
#include "Fwog/Texture.h"

#include <cstdint>
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
    FramebufferCache() = default;
    FramebufferCache(const FramebufferCache&) = delete;
    FramebufferCache& operator=(const FramebufferCache&) = delete;
    FramebufferCache(FramebufferCache&&) noexcept = default;
    FramebufferCache& operator=(FramebufferCache&&) noexcept = default;

    uint32_t CreateOrGetCachedFramebuffer(const RenderInfo& renderInfo);

    [[nodiscard]] std::size_t Size() const
    {
      return framebufferCacheKey_.size();
    }

    void Clear();

    ~FramebufferCache()
    {
      Clear();
    }

    void RemoveTexture(const Texture& texture);

  private:
    std::vector<RenderAttachments> framebufferCacheKey_;
    std::vector<uint32_t> framebufferCacheValue_;
  };
} // namespace Fwog::detail
