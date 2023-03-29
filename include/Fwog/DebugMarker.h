#pragma once
#include <Fwog/Config.h>

namespace Fwog
{
  /// @brief Use to demarcate a scope for viewing in a graphics debugger
  class ScopedDebugMarker
  {
  public:
    ScopedDebugMarker(const char* message);
    ~ScopedDebugMarker();

    ScopedDebugMarker(const ScopedDebugMarker&) = delete;
  };
} // namespace Fwog