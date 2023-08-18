#include <Fwog/Pipeline.h>
#include <Fwog/detail/ContextState.h>
#include <Fwog/detail/PipelineManager.h>

#include <utility>

#include FWOG_OPENGL_HEADER

namespace Fwog
{
  GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineInfo& info)
    : id_(detail::CompileGraphicsPipelineInternal(info))
      
  {
    detail::InvokeVerboseMessageCallback("Created graphics program with handle ", id_);
  }

  GraphicsPipeline::~GraphicsPipeline()
  {
    if (id_ != 0)
    {
      detail::InvokeVerboseMessageCallback("Destroyed graphics program with handle ", id_);
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

  ComputePipeline::ComputePipeline(const ComputePipelineInfo& info)
    : id_(detail::CompileComputePipelineInternal(info))
  {
    detail::InvokeVerboseMessageCallback("Created compute program with handle ", id_);
  }

  ComputePipeline::~ComputePipeline()
  {
    if (id_ != 0)
    {
      detail::InvokeVerboseMessageCallback("Destroyed compute program with handle ", id_);
      detail::DestroyComputePipelineInternal(id_);
    }
  }

  ComputePipeline::ComputePipeline(ComputePipeline&& old) noexcept
    : id_(std::exchange(old.id_, 0))
  {
  }

  ComputePipeline& ComputePipeline::operator=(ComputePipeline&& old) noexcept
  {
    if (this == &old)
    {
      return *this;
    }

    id_ = std::exchange(old.id_, 0);
    return *this;
  }

  Extent3D ComputePipeline::WorkgroupSize() const
  {
    return detail::GetComputePipelineInternal(id_)->workgroupSize;
  }
} // namespace Fwog