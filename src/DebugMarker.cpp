#include <Fwog/DebugMarker.h>

namespace Fwog
{
  ScopedDebugMarker::ScopedDebugMarker(const char* message)
  {
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
  }

  ScopedDebugMarker::~ScopedDebugMarker()
  {
    glPopDebugGroup();
  }
} // namespace Fwog