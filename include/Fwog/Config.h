#pragma once
#include <cassert>

#ifndef FWOG_ASSERT
  #define FWOG_ASSERT(x)   assert(x)
#endif

#ifndef FWOG_UNREACHABLE
  #define FWOG_UNREACHABLE FWOG_ASSERT(0)
#endif

#ifndef FWOG_CUSTOM_OPENGL_HEADER
  #include <glad/gl.h>
#else
  #include FWOG_CUSTOM_OPENGL_HEADER
#endif

#ifndef FWOG_DEFAULT_CLIP_DEPTH_RANGE_ZERO_TO_ONE
  #define FWOG_DEFAULT_CLIP_DEPTH_RANGE_NEGATIVE_ONE_TO_ONE
#endif
