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
    float f[4];
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
    bool clear;
  };

  struct Viewport
  {
    Rect2D drawRect;
    float minDepth;
    float maxDepth;
  };

  // describes the render targets that may be used in a draw
  struct RenderInfo
  {
    const Viewport* viewport;
    std::span<const RenderAttachment> colorAttachments;
    const RenderAttachment* depthAttachment;
    const RenderAttachment* stencilAttachment;
  };

  void BeginRendering(const RenderInfo& renderInfo);
  void EndRendering();

  namespace Cmd
  {
    void BindPipeline(const GraphicsPipelineInfo& pipeline);      // sets pipeline state
    void SetViewports(std::span<const Rect2D> viewports);         // glViewportArrayv
    
    // drawing operations
    void Draw(uint32_t vertexCount, uint32_t instanceCount,       // glDrawArraysInstancedBaseInstance
      uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, // glDrawElementsInstancedBaseVertexBaseInstance
      uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

    // vertex setup
    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride); // glVertexArrayVertexBuffer
    void BindIndexBuffer(const Buffer& buffer, IndexType indexType);
  }
}