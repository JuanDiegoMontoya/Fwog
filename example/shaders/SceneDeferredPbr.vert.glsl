#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_uv;
layout(location = 3) out vec4 v_curPos;
layout(location = 4) out vec4 v_oldPos;

layout(binding = 0, std140) uniform UBO0
{
  mat4 viewProj;
  mat4 oldViewProjUnjittered;
  mat4 viewProjUnjittered;
  mat4 invViewProj;
  mat4 proj;
  vec4 cameraPos;
};

struct ObjectUniforms
{
  mat4 model;
};

layout(binding = 1, std430) readonly buffer SSBO0
{
  ObjectUniforms objects[];
};

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
  int i = gl_InstanceID + gl_BaseInstance;
  v_position = (objects[i].model * vec4(a_pos, 1.0)).xyz;
  v_normal = normalize(inverse(transpose(mat3(objects[i].model))) * oct_to_float32x3(a_normal));
  v_uv = a_uv;
  gl_Position = viewProj * vec4(v_position, 1.0);
  v_curPos = viewProjUnjittered * vec4(v_position, 1.0);
  v_oldPos = oldViewProjUnjittered * vec4(v_position, 1.0);
}