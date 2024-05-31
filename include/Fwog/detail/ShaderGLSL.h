#pragma once
#include <Fwog/Shader.h>

namespace Fwog::detail
{
  [[nodiscard]] uint32_t CompileShaderGLSL(PipelineStage stage, std::string_view sourceGLSL);
} // namespace Fwog::detail