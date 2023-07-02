#include <Fwog/Context.h>
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

  ComputePipeline::ComputePipeline(const ComputePipelineInfo& info) : id_(detail::CompileComputePipelineInternal(info))
  {
    GLint workgroupSize[3];
    glGetProgramiv(static_cast<GLuint>(id_), GL_COMPUTE_WORK_GROUP_SIZE, workgroupSize);

    FWOG_ASSERT(workgroupSize[0] <= GetDeviceProperties().limits.maxComputeWorkGroupSize[0] &&
                workgroupSize[1] <= GetDeviceProperties().limits.maxComputeWorkGroupSize[1] &&
                workgroupSize[2] <= GetDeviceProperties().limits.maxComputeWorkGroupSize[2]);
    FWOG_ASSERT(workgroupSize[0] * workgroupSize[1] * workgroupSize[2] <=
                GetDeviceProperties().limits.maxComputeWorkGroupInvocations);

    workgroupSize_.width = static_cast<uint32_t>(workgroupSize[0]);
    workgroupSize_.height = static_cast<uint32_t>(workgroupSize[1]);
    workgroupSize_.depth = static_cast<uint32_t>(workgroupSize[2]);

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
    : id_(std::exchange(old.id_, 0)),
      workgroupSize_(std::exchange(old.workgroupSize_, Extent3D{}))
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
} // namespace Fwog