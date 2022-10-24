#pragma once
#include <Fwog/BasicTypes.h>
#include <array>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

namespace Fwog
{
  // clang-format off
  class Texture;
  class Sampler;
  class Buffer;
  struct GraphicsPipeline;
  struct ComputePipeline;

  struct ClearColorValue
  {
    ClearColorValue() {};

    template<typename... Args>
    requires (sizeof...(Args) <= 4)
    ClearColorValue(const Args&... args)
      : data(std::array<std::common_type_t<std::remove_cvref_t<Args>...>, 4>{ args...})
    {
    }

    std::variant<std::array<float, 4>, std::array<uint32_t, 4>, std::array<int32_t, 4>> data;
  };

  struct ClearDepthStencilValue
  {
    float depth{};
    int32_t stencil;
  };

  using ClearValue = std::variant<ClearColorValue, ClearDepthStencilValue>;

  struct RenderAttachment
  {
    const Texture* texture = nullptr;
    ClearValue clearValue;
    bool clearOnLoad = false;
  };

  // TODO: use these structs
  //struct RenderColorAttachment
  //{
  //  const Texture* texture = nullptr;
  //  bool clearOnLoad = false;
  //  ClearColorValue clearValue;
  //};
  //
  //struct RenderDepthStencilAttachment
  //{
  //  const Texture* texture = nullptr;
  //  bool clearOnLoad = false;
  //  ClearDepthStencilValue clearValue;
  //};

  struct Viewport
  {
    Rect2D drawRect = {};  // glViewport
    float minDepth = 0.0f; // glDepthRangef
    float maxDepth = 1.0f; // glDepthRangef

    bool operator==(const Viewport&) const noexcept = default;
  };

  // I don't know how to get the default framebuffer's textures so I have this awful struct instead
  struct SwapchainRenderInfo
  {
    std::string_view name;
    Viewport viewport = {};
    bool clearColorOnLoad = false;
    ClearColorValue clearColorValue;
    bool clearDepthOnLoad = false;
    float clearDepthValue = 0.0f;
    bool clearStencilOnLoad = false;
    int32_t clearStencilValue = 0;
  };

  // Describes the render targets that may be used in a draw
  struct RenderInfo
  {
    std::string_view name;
    // If null, the viewport size will be the minimum the render targets' size, and the offset will be 0
    const Viewport* viewport = nullptr;
    std::span<const RenderAttachment> colorAttachments;
    const RenderAttachment* depthAttachment = nullptr;
    const RenderAttachment* stencilAttachment = nullptr;
  };

  // begin or end a scope of rendering to a set of render targets
  void BeginSwapchainRendering(const SwapchainRenderInfo& renderInfo);
  void BeginRendering(const RenderInfo& renderInfo);
  void EndRendering();

  // begin a compute scope
  void BeginCompute(std::string_view name = {});
  void EndCompute();

  void BlitTexture(const Texture& source,
                   const Texture& target,
                   Offset3D sourceOffset,
                   Offset3D targetOffset,
                   Extent3D sourceExtent,
                   Extent3D targetExtent,
                   Filter filter,
                   AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);

  // blit to 0
  void BlitTextureToSwapchain(const Texture& source,
                              Offset3D sourceOffset,
                              Offset3D targetOffset,
                              Extent3D sourceExtent,
                              Extent3D targetExtent,
                              Filter filter,
                              AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);

  void CopyTexture(const Texture& source,
                   const Texture& target,
                   uint32_t sourceLevel,
                   uint32_t targetLevel,
                   Offset3D sourceOffset,
                   Offset3D targetOffset,
                   Extent3D extent);

  // Cmd:: functions can only be called within a rendering scope
  namespace Cmd
  {
    void BindGraphicsPipeline(const GraphicsPipeline& pipeline); // sets pipeline state
    void BindComputePipeline(const ComputePipeline& pipeline);

    // dynamic state
    // void SetViewports(std::span<const Rect2D> viewports);         // glViewportArrayv
    void SetViewport(const Viewport& viewport); // glViewport

    void SetScissor(const Rect2D& scissor); // glScissor

    // drawing operations

    // glDrawArraysInstancedBaseInstance
    void Draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance);

    // glDrawElementsInstancedBaseVertexBaseInstance
    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance);

    // glMultiDrawArraysIndirect
    void DrawIndirect(const Buffer& commandBuffer,
                      uint64_t commandBufferOffset,
                      uint32_t drawCount,
                      uint32_t stride);

    // glMultiDrawArraysIndirectCount
    void DrawIndirectCount(const Buffer& commandBuffer,
                           uint64_t commandBufferOffset,
                           const Buffer& countBuffer,
                           uint64_t countBufferOffset,
                           uint32_t maxDrawCount,
                           uint32_t stride);

    // glMultiDrawElementsIndirect
    void DrawIndexedIndirect(const Buffer& commandBuffer,
                             uint64_t commandBufferOffset,
                             uint32_t drawCount,
                             uint32_t stride);

    // glMultiDrawElementsIndirectCount
    void DrawIndexedIndirectCount(const Buffer& commandBuffer,
                                  uint64_t commandBufferOffset,
                                  const Buffer& countBuffer,
                                  uint64_t countBufferOffset,
                                  uint32_t maxDrawCount,
                                  uint32_t stride);

    // vertex setup

    // glVertexArrayVertexBuffer
    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride);
    
    // glVertexArrayElementBuffer
    void BindIndexBuffer(const Buffer& buffer, IndexType indexType);

    // 'descriptors'
    // valid in render and compute scopes
    
    // glBindBufferRange
    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);

    // glBindBufferRange
    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);

    // glBindTextureUnit + glBindSampler
    void BindSampledImage(uint32_t index, const Texture& texture, const Sampler& sampler);

    // glBindImageTexture{s}
    void BindImage(uint32_t index, const Texture& texture, uint32_t level);

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset);
    void MemoryBarrier(MemoryBarrierAccessBits accessBits);

    // clang-format on
  } // namespace Cmd
} // namespace Fwog