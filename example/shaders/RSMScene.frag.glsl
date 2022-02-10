#version 460 core

layout(location = 0) out vec3 o_flux;
layout(location = 1) out vec3 o_normal;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

layout(binding = 1, std140) uniform UBO1
{
  vec4 viewPos;
  mat4 sunViewProj;
  vec4 sunDir;
  vec4 sunStrength;
}shadingUniforms;

void main()
{
  o_flux = vec3(v_uv, 0) * shadingUniforms.sunStrength.rgb;
  o_normal = v_normal;
}