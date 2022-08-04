#pragma once
#include <Fwog/BasicTypes.h>
#include <span>
#include <string_view>

namespace Fwog
{
  class Texture;
  class Sampler;
  class Buffer;
  struct GraphicsPipeline;
  struct ComputePipeline;

  struct ClearColorValue
  {
    union
    {
      float f[4];
      uint32_t ui[4];
      int32_t i[4];
    };
  };

  struct ClearDepthStencilValue
  {
    float depth{};
    int32_t stencil;
  };

  union ClearValue
  {
    ClearColorValue color;
    ClearDepthStencilValue depthStencil;
  };

  struct RenderAttachment
  {
    const Texture* texture = nullptr;
    ClearValue clearValue;
    bool clearOnLoad = false;
  };

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

  // describes the render targets that may be used in a draw
  struct RenderInfo
  {
    std::string_view name;
    // if null, the viewport size will be automatically deduced based on the render targets
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

  void BlitTexture(
    const Texture& source,
    const Texture& target,
    Offset3D sourceOffset,
    Offset3D targetOffset,
    Extent3D sourceExtent,
    Extent3D targetExtent,
    Filter filter,
    AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);

  // blit to 0
  void BlitTextureToSwapchain(
    const Texture& source,
    Offset3D sourceOffset,
    Offset3D targetOffset,
    Extent3D sourceExtent,
    Extent3D targetExtent,
    Filter filter,
    AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);

  void CopyTexture(
    const Texture& source,
    const Texture& target,
    uint32_t sourceLevel,
    uint32_t targetLevel,
    Offset3D sourceOffset,
    Offset3D targetOffset,
    Extent3D extent);

  // Cmd:: functions can only be called within a rendering scope
  namespace Cmd
  {
    void BindGraphicsPipeline(GraphicsPipeline pipeline);         // sets pipeline state
    void BindComputePipeline(ComputePipeline pipeline);

    // dynamic state
    //void SetViewports(std::span<const Rect2D> viewports);         // glViewportArrayv
    void SetViewport(const Viewport& viewport);                  // glViewport
    
    void SetScissor(const Rect2D& scissor);                      // glScissor

    // drawing operations
    void Draw(uint32_t vertexCount, uint32_t instanceCount,       // glDrawArraysInstancedBaseInstance
      uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, // glDrawElementsInstancedBaseVertexBaseInstance
      uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
    void DrawIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, 
      uint32_t drawCount, uint32_t stride);
    void DrawIndirectCount(const Buffer& commandBuffer, uint64_t commandBufferOffset, 
      const Buffer& countBuffer, uint64_t countBufferOffset, 
      uint32_t maxDrawCount, uint32_t stride);
    void DrawIndexedIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset, 
      uint32_t drawCount, uint32_t stride);
    void DrawIndexedIndirectCount(const Buffer& commandBuffer, uint64_t commandBufferOffset, 
      const Buffer& countBuffer, uint64_t countBufferOffset, 
      uint32_t maxDrawCount, uint32_t stride);

    // vertex setup
    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride); // glVertexArrayVertexBuffer
    void BindIndexBuffer(const Buffer& buffer, IndexType indexType);                                      // glVertexArrayElementBuffer

    // 'descriptors'
    // valid in render and compute scopes
    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);         // glBindBufferRange
    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);         // glBindBufferRange
    void BindSampledImage(uint32_t index, const Texture& texture, const Sampler& sampler); // glBindTextureUnit + glBindSampler
    void BindImage(uint32_t index, const Texture& texture, uint32_t level);                       // glBindImageTexture{s}

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset);
    void MemoryBarrier(MemoryBarrierAccessBits accessBits);
  }
}