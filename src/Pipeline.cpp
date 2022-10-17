#include <Fwog/Pipeline.h>
#include <Fwog/detail/PipelineManager.h>

namespace Fwog
{
  GraphicsPipeline CompileGraphicsPipeline(const GraphicsPipelineInfo& info)
  {
    return detail::CompileGraphicsPipelineInternal(info);
  }

  bool DestroyGraphicsPipeline(GraphicsPipeline pipeline)
  {
    return detail::DestroyGraphicsPipelineInternal(pipeline);
  }

  ComputePipeline CompileComputePipeline(const ComputePipelineInfo& info)
  {
    return detail::CompileComputePipelineInternal(info);
  }

  bool DestroyComputePipeline(ComputePipeline pipeline)
  {
    return detail::DestroyComputePipelineInternal(pipeline);
  }
} // namespace Fwog