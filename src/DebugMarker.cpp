#include <gsdf/Common.h>
#include <gsdf/DebugMarker.h>

namespace GFX
{
  ScopedDebugMarker::ScopedDebugMarker(const char* message)
  {
    glPushDebugGroup(
      GL_DEBUG_SOURCE_APPLICATION,
      0,
      -1,
      message);
  }

  ScopedDebugMarker::~ScopedDebugMarker()
  {
    glPopDebugGroup();
  }
}