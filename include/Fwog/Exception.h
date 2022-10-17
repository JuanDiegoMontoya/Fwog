#pragma once
#include <exception>
#include <string>
#include <utility>

namespace Fwog
{
  class Exception : public std::exception
  {
  public:
    Exception() {}
    Exception(std::string message) : message_(std::move(message)) {}

    const char* what() const noexcept override
    {
      return message_.c_str();
    }

  protected:
    std::string message_;
  };

  class ShaderCompilationException : public Exception
  {
    using Exception::Exception;
  };

  class PipelineCompilationException : public Exception
  {
    using Exception::Exception;
  };
} // namespace Fwog