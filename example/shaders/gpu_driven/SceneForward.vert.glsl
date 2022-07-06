#version 460 core
#extension GL_GOOGLE_include_directive : enable

#include "Common.h"

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_uv;
layout(location = 3) out uint v_materialIdx;

vec2 signNotZero(vec2 v)
{
  return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

vec3 oct_to_float32x3(vec2 e)
{
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

void main()
{
  uint i = objectIndices.array[gl_DrawID];
  v_materialIdx = objects[i].materialIdx;
  v_position = (objects[i].model * vec4(a_pos, 1.0)).xyz;
  v_normal = normalize(inverse(transpose(mat3(objects[i].model))) * oct_to_float32x3(a_normal));
  v_uv = a_uv;
  gl_Position = globalUniforms.viewProj * vec4(v_position, 1.0);
}