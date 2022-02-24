#include <gsdf/Pipeline.h>
#include <gsdf/detail/PipelineManager.h>

namespace GFX
{
  std::optional<GraphicsPipeline> CompileGraphicsPipeline(const GraphicsPipelineInfo& info)
  {
    return detail::CompileGraphicsPipelineInternal(info);
  }

  bool DestroyGraphicsPipeline(GraphicsPipeline pipeline)
  {
    return detail::DestroyGraphicsPipelineInternal(pipeline);
  }

  std::optional<ComputePipeline> CompileComputePipeline(const ComputePipelineInfo& info)
  {
    return detail::CompileComputePipelineInternal(info);
  }

  bool DestroyComputePipeline(ComputePipeline pipeline)
  {
    return detail::DestroyComputePipelineInternal(pipeline);
  }
}