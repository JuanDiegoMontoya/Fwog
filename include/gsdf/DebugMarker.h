#pragma once

namespace GFX
{
  class ScopedDebugMarker
  {
  public:
    ScopedDebugMarker(const char* message);
    ~ScopedDebugMarker();

    ScopedDebugMarker(const ScopedDebugMarker&) = delete;
  };
}