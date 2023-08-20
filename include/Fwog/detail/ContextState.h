#pragma once
#include <Fwog/Context.h>

#include <Fwog/BasicTypes.h>
#include <Fwog/detail/FramebufferCache.h>
#include <Fwog/detail/PipelineManager.h>
#include <Fwog/detail/SamplerCache.h>
#include <Fwog/detail/VertexArrayCache.h>

#include <sstream>
#include <memory>
#include <string_view>

#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  constexpr int MAX_COLOR_ATTACHMENTS = 8;

  struct ContextState
  {
    DeviceProperties properties;

    void (*verboseMessageCallback)(std::string_view) = nullptr;
    void (*renderToSwapchainHook)(const SwapchainRenderInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*renderHook)(const RenderInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*renderNoAttachmentsHook)(const RenderNoAttachmentsInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*computeHook)(std::string_view name, const std::function<void()>& func) = nullptr;

    // Used for scope error checking
    bool isComputeActive = false;
    bool isRendering = false;

    // Used for error checking for indexed draws
    bool isIndexBufferBound = false;

    // Currently unused
    bool isRenderingToSwapchain = false;

    // True during a render or compute scope that has a name.
    bool isScopedDebugGroupPushed = false;

    // True when a pipeline with a name is bound during a render or compute scope.
    bool isPipelineDebugGroupPushed = false;

    // True during SwapchainRendering scopes that disable sRGB.
    // This is needed since regular Rendering scopes always have framebuffer sRGB enabled
    // (the user uses framebuffer attachments to decide if they want the linear->sRGB conversion).
    bool srgbWasDisabled = false;

    // Stores a pointer to the previously bound graphics pipeline state. This is used for state deduplication.
    // A shared_ptr is needed as the user can delete pipelines at any time, but we need to ensure it stays alive until
    // the next pipeline is bound.
    std::shared_ptr<const detail::GraphicsPipelineInfoOwning> lastGraphicsPipeline{};
    bool lastPipelineWasCompute = false;

    std::shared_ptr<const detail::ComputePipelineInfoOwning> lastComputePipeline{};

    // Currently unused (and probably shouldn't be used)
    const RenderInfo* lastRenderInfo{};

    // These can be set at the start of rendering, so they need to be tracked separately from the other pipeline state.
    std::array<ColorComponentFlags, MAX_COLOR_ATTACHMENTS> lastColorMask = {};
    bool lastDepthMask = true;
    uint32_t lastStencilMask[2] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    bool initViewport = true;
    Viewport lastViewport = {};
    Rect2D lastScissor = {};
    bool scissorEnabled = false;

    // Potentially used for state deduplication.
    GLuint currentVao = 0;
    GLuint currentFbo = 0;

    // These persist until another Pipeline is bound.
    // They are not used for state deduplication, as they are arguments for GL draw calls.
    PrimitiveTopology currentTopology{};
    IndexType currentIndexType{};

    detail::FramebufferCache fboCache;
    detail::VertexArrayCache vaoCache;
    detail::SamplerCache samplerCache;
  } inline* context = nullptr;

  // Clears all resource bindings.
  // This is called at the beginning of rendering/compute scopes 
  // or when the pipeline state has been invalidated, but only in debug mode.
  void ZeroResourceBindings();

  // Prints a formatted message to a stringstream, then
  // invokes the message callback with the formatted message
  template<class... Args>
  void InvokeVerboseMessageCallback(Args&&... args)
  {
    if (context->verboseMessageCallback != nullptr)
    {
      // This probably allocates, but at least it works
      std::stringstream stream;
      ((stream << args), ...);
      context->verboseMessageCallback(stream.str().c_str());
    }
  }
} // namespace Fwog::detail