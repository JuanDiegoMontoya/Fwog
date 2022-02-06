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
}