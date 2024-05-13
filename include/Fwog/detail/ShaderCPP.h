#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Fwog::detail
{
  std::string CompileShaderCppToGlsl(const std::filesystem::path& path);
  std::string CompileShaderCppToGlsl(std::string_view sourceCPP);
} // namespace Fwog::detail