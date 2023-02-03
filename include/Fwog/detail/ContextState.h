#pragma once
#include <Fwog/Context.h>

#include <Fwog/BasicTypes.h>
#include <Fwog/detail/FramebufferCache.h>
#include <Fwog/detail/PipelineManager.h>
#include <Fwog/detail/SamplerCache.h>
#include <Fwog/detail/VertexArrayCache.h>
#include <memory>

namespace Fwog::detail
{
  constexpr int MAX_COLOR_ATTACHMENTS = 8;

  struct ContextState
  {
    DeviceProperties properties;

    bool isComputeActive = false;
    bool isRendering = false;
    bool isIndexBufferBound = false;
    bool isRenderingToSwapchain = false;
    bool isScopedDebugGroupPushed = false;
    bool isPipelineDebugGroupPushed = false;
    bool srgbWasDisabled = false;

    // TODO: way to reset this pointer in case the user wants to do their own OpenGL operations (invalidate the cache).
    // A shared_ptr is needed as the user can delete pipelines at any time, but we need to ensure it stays alive until
    // the next pipeline is bound.
    std::shared_ptr<const detail::GraphicsPipelineInfoOwning> lastGraphicsPipeline{};
    const RenderInfo* lastRenderInfo{};

    // these can be set at the start of rendering, so they need to be tracked separately from the other pipeline state
    std::array<ColorComponentFlags, MAX_COLOR_ATTACHMENTS> lastColorMask = {};
    bool lastDepthMask = true;
    uint32_t lastStencilMask[2] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    bool initViewport = true;
    Viewport lastViewport = {};
    Rect2D lastScissor = {};
    bool scissorEnabled = false;

    PrimitiveTopology currentTopology{};
    IndexType currentIndexType{};
    GLuint currentVao = 0;
    GLuint currentFbo = 0;

    detail::FramebufferCache fboCache;
    detail::VertexArrayCache vaoCache;
    detail::SamplerCache samplerCache;
  } inline* context = nullptr;
} // namespace Fwog::detail