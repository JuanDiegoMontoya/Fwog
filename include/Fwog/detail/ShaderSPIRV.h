#pragma once
#include <Fwog/Shader.h>

namespace Fwog::detail
{
  [[nodiscard]] uint32_t CompileShaderSpirv(PipelineStage stage, const ShaderSpirvInfo& spirvInfo);
} // namespace Fwog::detail