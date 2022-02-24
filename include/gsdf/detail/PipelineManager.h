#pragma once
#include <gsdf/Pipeline.h>
#include <optional>
#include <vector>

namespace GFX::detail
{
  struct VertexInputStateOwning
  {
    std::vector<VertexInputBindingDescription> vertexBindingDescriptions;
  };
  struct ColorBlendStateOwning
  {
    bool logicOpEnable;
    LogicOp logicOp;
    std::vector<ColorBlendAttachmentState> attachments;
    float blendConstants[4];
  };
  struct GraphicsPipelineInfoOwning
  {
    uint32_t shaderProgram;
    InputAssemblyState inputAssemblyState;
    VertexInputStateOwning vertexInputState;
    RasterizationState rasterizationState;
    DepthStencilState depthStencilState;
    ColorBlendStateOwning colorBlendState;
  };

  std::optional<GraphicsPipeline> CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info);
  const GraphicsPipelineInfoOwning* GetGraphicsPipelineInternal(GraphicsPipeline pipeline);
  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline);

  std::optional<ComputePipeline> CompileComputePipelineInternal(const ComputePipelineInfo& info);
  const ComputePipelineInfo* GetComputePipelineInternal(ComputePipeline pipeline);
  bool DestroyComputePipelineInternal(ComputePipeline pipeline);
}