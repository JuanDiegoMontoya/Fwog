#pragma once
#include <Fwog/Config.h>
#include <exception>
#include <string>
#include <utility>

namespace Fwog
{
  /// @brief Base type for all exceptions
  class Exception : public std::exception
  {
  public:
    Exception() = default;
    explicit Exception(std::string message) : message_(std::move(message)) {}

    [[nodiscard]] const char* what() const noexcept override
    {
      return message_.c_str();
    }

  protected:
    std::string message_;
  };

  /// @brief Exception type thrown when a shader encounters a compilation error
  ///
  /// The exception string will contain the error message
  class ShaderCompilationException : public Exception
  {
    using Exception::Exception;
  };

  /// @brief Exception type thrown when a pipeline encounters a compilation error
  ///
  /// These can be thrown if OpenGL encounters a linker error when linking two or more shaders into a program.
  class PipelineCompilationException : public Exception
  {
    using Exception::Exception;
  };
} // namespace Fwog