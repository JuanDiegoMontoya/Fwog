#version 460 core

layout(location = 0) in vec3 a_pos;

layout(binding = 0, std140) uniform UBO0
{
  mat4 viewProj;
};

struct ObjectUniforms
{
  mat4 model;
};

layout(binding = 1, std430) readonly buffer SSBO0
{
  ObjectUniforms objects[];
};

void main()
{
  int i = gl_InstanceID + gl_BaseInstance;
  gl_Position = viewProj * objects[i].model * vec4(a_pos, 1.0);
}