#version 460 core
#extension GL_GOOGLE_include_directive : enable

#include "Common.h"

layout(location = 0) out uint v_drawID;

// 14-vertex CCW triangle strip
vec3 CreateCube(in uint vertexID)
{
  uint b = 1 << vertexID;
  return vec3((0x287a & b) != 0, (0x02af & b) != 0, (0x31e3 & b) != 0);
}

void main()
{
  uint i = objectIndices.array[gl_BaseInstance + gl_InstanceID];
  v_drawID = gl_BaseInstance + gl_InstanceID;
  vec3 a_pos = CreateCube(gl_VertexID) - .5; // gl_VertexIndex for Vulkan
  ObjectUniforms obj = objects[i];
  a_pos *= boundingBoxes[i].halfExtent * 2.0 + 1e-1;
  a_pos += boundingBoxes[i].offset;
  vec3 position = (obj.model * vec4(a_pos, 1.0)).xyz;
  gl_Position = globalUniforms.viewProj * vec4(position, 1.0);
}