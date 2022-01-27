#pragma once
#include <cstdint>

// note: baseInstance is for glMultiDraw*Indirect ONLY
// for any other purpose it must be zero

struct DrawElementsIndirectCommand
{
  uint32_t count{ 0 };        // num indices in draw call
  uint32_t instanceCount{ 0 };// num instances in draw call
  uint32_t firstIndex{ 0 };   // offset in index buffer: sizeof(element)*firstIndex from start of buffer
  uint32_t baseVertex{ 0 };   // offset in vertex buffer: sizeof(vertex)*baseVertex from start of buffer
  uint32_t baseInstance{ 0 }; // first instance to draw (position in instanced buffer)
};

struct DrawArraysIndirectCommand
{
  uint32_t count{ 0 };
  uint32_t instanceCount{ 0 };
  uint32_t first{ 0 };        // equivalent to baseVertex from previous struct
  uint32_t baseInstance{ 0 };
};