#pragma once
#include <Fwog/Pipeline.h>
#include <vector>
#include <memory>
#include <string>

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
    std::string name;
    InputAssemblyState inputAssemblyState;
    VertexInputStateOwning vertexInputState;
    RasterizationState rasterizationState;
    DepthState depthState;
    StencilState stencilState;
    ColorBlendStateOwning colorBlendState;
  };

  struct ComputePipelineInfoOwning
  {
    std::string name;
  };

  GraphicsPipeline CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info);
  std::shared_ptr<const GraphicsPipelineInfoOwning> GetGraphicsPipelineInternal(GraphicsPipeline pipeline);
  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline);

  ComputePipeline CompileComputePipelineInternal(const ComputePipelineInfo& info);
  std::shared_ptr<const ComputePipelineInfoOwning> GetComputePipelineInternal(ComputePipeline pipeline);
  bool DestroyComputePipelineInternal(ComputePipeline pipeline);
}