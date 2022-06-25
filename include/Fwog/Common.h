#pragma once
#include <cassert>

#define FWOG_ASSERT(x) assert(x)
#define FWOG_UNREACHABLE FWOG_ASSERT(0)

// TODO: figure a way to make this include optional (in case the user wants to provide gl functions)
// probably by using a define
#include <glad/gl.h>