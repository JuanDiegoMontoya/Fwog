#pragma once
#include <fwog/Pipeline.h>
#include <optional>
#include <vector>
#include <memory>

namespace Fwog::detail
{
  // owning versions of pipeline info structs so we don't lose references
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
    InputAssemblyState inputAssemblyState;
    VertexInputStateOwning vertexInputState;
    RasterizationState rasterizationState;
    DepthState depthState;
    StencilState stencilState;
    ColorBlendStateOwning colorBlendState;
  };
  constexpr int joe = sizeof(GraphicsPipelineInfoOwning);
  std::optional<GraphicsPipeline> CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info);
  std::shared_ptr<const GraphicsPipelineInfoOwning> GetGraphicsPipelineInternal(GraphicsPipeline pipeline);
  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline);

  std::optional<ComputePipeline> CompileComputePipelineInternal(const ComputePipelineInfo& info);
  bool DestroyComputePipelineInternal(ComputePipeline pipeline);
}