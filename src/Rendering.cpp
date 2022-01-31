#include <gsdf/Rendering.h>
#include <gsdf/Common.h>

namespace GFX
{
  // rendering cannot be suspended/resumed, nor done on multiple threads
  // since only one rendering instance can be active at a time, we store some state here
  namespace
  {
    bool isRendering = false;
  }

  void BeginRendering(const RenderInfo& renderInfo)
  {
    GSDF_ASSERT(!isRendering && "Cannot call BeginRendering when rendering");
    isRendering = true;
  }

  void EndRendering()
  {
    GSDF_ASSERT(isRendering && "Cannot call EndRendering when not rendering");
    isRendering = false;
  }

  namespace Cmd
  {

  }
}