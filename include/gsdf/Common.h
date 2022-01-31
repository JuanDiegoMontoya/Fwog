#pragma once
#include <cassert>

#define GSDF_ASSERT(x) assert(x)
#define GSDF_UNREACHABLE GSDF_ASSERT(0)

#include <glad/gl.h>