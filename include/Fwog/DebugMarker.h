#pragma once
#include <Fwog/Config.h>

namespace Fwog
{
  class ScopedDebugMarker
  {
  public:
    ScopedDebugMarker(const char* message);
    ~ScopedDebugMarker();

    ScopedDebugMarker(const ScopedDebugMarker&) = delete;
  };
} // namespace Fwog