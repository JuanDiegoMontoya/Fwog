#pragma once

namespace Fwog
{
  class ScopedDebugMarker
  {
  public:
    ScopedDebugMarker(const char* message);
    ~ScopedDebugMarker();

    ScopedDebugMarker(const ScopedDebugMarker&) = delete;
  };
}