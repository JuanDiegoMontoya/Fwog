#pragma once

namespace GFX
{
  class DebugMarker
  {
  public:
    DebugMarker(const char* message);
    ~DebugMarker();

    DebugMarker(const DebugMarker&) = delete;
  };
}