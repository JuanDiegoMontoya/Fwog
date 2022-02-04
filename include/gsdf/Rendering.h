#pragma once
#include <gsdf/BasicTypes.h>

// these can probably be forward declarations instead
#include <gsdf/Texture.h>
#include <gsdf/Pipeline.h>
#include <gsdf/Buffer.h>

#include <span>
#include <optional>

namespace GFX
{
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
    TextureView* textureView;
    ClearValue clearValue;
    bool clearOnLoad;
  };

  struct Viewport
  {
    Rect2D drawRect; // glViewport
    float minDepth;  // glDepthRangef
    float maxDepth;  // glDepthRangef
  };

  // I don't know how to get the default framebuffer's textures so I have this awful struct instead
  struct SwapchainRenderInfo
  {
    const Viewport* viewport;
    bool clearColorOnLoad;
    ClearColorValue clearColorValue;
    bool clearDepthOnLoad;
    float clearDepthValue;
    bool clearStencilOnLoad;
    int32_t clearStencilValue;
  };

  // describes the render targets that may be used in a draw
  struct RenderInfo
  {
    const Viewport* viewport;
    std::span<const RenderAttachment> colorAttachments;
    const RenderAttachment* depthAttachment;
    const RenderAttachment* stencilAttachment;
  };

  void BeginSwapchainRendering(const SwapchainRenderInfo& renderInfo);
  void BeginRendering(const RenderInfo& renderInfo);
  void EndRendering();

  // Cmd:: functions can only be called within a rendering context
  namespace Cmd
  {
    void BindPipeline(const GraphicsPipelineInfo& pipeline);      // sets pipeline state
    
    // dynamic state
    void SetViewports(std::span<const Rect2D> viewports);         // glViewportArrayv
    
    // drawing operations
    void Draw(uint32_t vertexCount, uint32_t instanceCount,       // glDrawArraysInstancedBaseInstance
      uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, // glDrawElementsInstancedBaseVertexBaseInstance
      uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

    // vertex setup
    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride); // glVertexArrayVertexBuffer
    void BindIndexBuffer(const Buffer& buffer, IndexType indexType);                                      // glVertexArrayElementBuffer

    // 'descriptors'
    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);         // glBindBufferRange
    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset, uint64_t size);         // glBindBufferRange
    void BindSampledImage(uint32_t index, const TextureView& textureView, const TextureSampler& sampler); // glBindTextureUnit + glBindSampler
    void BindImage(uint32_t index, const TextureView& textureView, uint32_t level);                       // glBindImageTexture{s}
  }
}