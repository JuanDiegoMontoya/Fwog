#pragma once
#include <gsdf/Texture.h>
#include <gsdf/BasicTypes.h>
#include <span>
#include <optional>

namespace GFX
{
  struct ClearColorValue
  {
    float f[4];
  };

  struct ClearDepthStencilValue
  {
    float depth{};
    int32_t stencil{};
  };

  union ClearValue
  {
    ClearColorValue color;
    ClearDepthStencilValue depthStencil;
  };

  struct RenderAttachment
  {
    TextureView* textureView{ nullptr };
    ClearValue clearValue;
    bool clear{ false };
  };

  // This structure describes the render targets that may be used in a draw.
  // Inspired by VkRenderingInfoKHR of Vulkan's dynamic rendering extension.
  struct RenderInfo
  {
    Offset2D offset{};
    Extent2D size{};
    std::span<std::optional<RenderAttachment>> colorAttachments;
    std::optional<RenderAttachment> depthAttachment{ std::nullopt };
    std::optional<RenderAttachment> stencilAttachment{ std::nullopt };
  };

  void BeginRendering(const RenderInfo&);
  void EndRendering();
}