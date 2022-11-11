#include <Fwog/Pipeline.h>
#include <Fwog/detail/PipelineManager.h>

#include <utility>

namespace Fwog
{
  GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineInfo& info)
      : id_(detail::CompileGraphicsPipelineInternal(info))
  {
  }

  GraphicsPipeline::~GraphicsPipeline()
  {
    if (id_ != 0)
    {
      detail::DestroyGraphicsPipelineInternal(id_);
    }
  }

  GraphicsPipeline::GraphicsPipeline(GraphicsPipeline&& old) noexcept : id_(std::exchange(old.id_, 0)) {}

  GraphicsPipeline& GraphicsPipeline::operator=(GraphicsPipeline&& old) noexcept
  {
    if (this == &old)
    {
      return *this;
    }

    id_ = std::exchange(old.id_, 0);
    return *this;
  }

  ComputePipeline::ComputePipeline(const ComputePipelineInfo& info) : id_(detail::CompileComputePipelineInternal(info))
  {
  }

  ComputePipeline::~ComputePipeline()
  {
    if (id_ != 0)
    {
      detail::DestroyComputePipelineInternal(id_);
    }
  }
  ComputePipeline::ComputePipeline(ComputePipeline&& old) noexcept : id_(std::exchange(old.id_, 0)) {}

  ComputePipeline& ComputePipeline::operator=(ComputePipeline&& old) noexcept
  {
    if (this == &old)
    {
      return *this;
    }

    id_ = std::exchange(old.id_, 0);
    return *this;
  }
} // namespace Fwog