#pragma once
#include <cassert>

#define GSDF_ASSERT(x) assert(x)
#define GSDF_UNREACHABLE GSDF_ASSERT(0)

// TODO: figure a way to make this include optional (in case the user wants to provide gl functions)
// probably by using a define
#include <glad/gl.h>