#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_uv;

layout(binding = 0, std140) uniform Uniforms
{
  mat4 model;
  mat4 viewProj;
}uniforms;

void main()
{
  v_position = (uniforms.model * vec4(a_pos, 1.0)).xyz;
  v_normal = (uniforms.model * vec4(a_normal, 0.0)).xyz;
  v_uv = a_uv;
  gl_Position = uniforms.viewProj * vec4(v_position, 1.0);
}