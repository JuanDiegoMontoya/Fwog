#pragma once
#include <Fwog/Shader.h>

namespace Fwog::detail
{
  uint32_t CompileShaderGLSL(PipelineStage stage, std::string_view sourceGLSL, std::string_view name);
} // namespace Fwog::detail