#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include "BasicTypes.h"

namespace GFX
{
  class TextureView;

  class Framebuffer
  {
  public:
    static std::optional<Framebuffer> Create();

    Framebuffer(Framebuffer&& old) noexcept;
    Framebuffer& operator=(Framebuffer&& old) noexcept;
    ~Framebuffer();

    void SetAttachment(Attachment slot, const TextureView& view, uint32_t level);
    void ResetAttachment(Attachment slot);
    void SetDrawBuffers(std::span<const Attachment> slots);
    void Bind();

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] uint32_t Handle() const { return handle_; }
    [[nodiscard]] uint32_t GetAttachmentAPIHandle(Attachment slot) const;

    static void Blit(const Framebuffer& source, const Framebuffer& target,
      Rect2D sourceRect, Rect2D targetRect,
      AspectMask mask, Filter filter);

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

  private:
    Framebuffer();
    uint32_t handle_{};
  };
}