#pragma once
#include <cassert>

#if (!defined(FWOG_DEBUG) && !defined(NDEBUG)) || defined(FWOG_FORCE_DEBUG)
  #define FWOG_DEBUG
#endif

#ifndef FWOG_ASSERT
  #ifdef FWOG_DEBUG
    #define FWOG_ASSERT(x) assert(x)
  #else
    #define FWOG_ASSERT(x) (void)(x)
  #endif
#endif

#ifndef FWOG_UNREACHABLE
  #define FWOG_UNREACHABLE FWOG_ASSERT(0)
#endif

#ifndef FWOG_OPENGL_HEADER
  #define FWOG_OPENGL_HEADER <glad/gl.h>
#endif

#ifndef FWOG_DEFAULT_CLIP_DEPTH_RANGE_ZERO_TO_ONE
  #define FWOG_DEFAULT_CLIP_DEPTH_RANGE_NEGATIVE_ONE_TO_ONE
#endif