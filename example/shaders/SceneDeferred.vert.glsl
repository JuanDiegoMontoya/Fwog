#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;

layout(location = 0) out vec3 v_color;

layout(binding = 0, std140) uniform Uniforms
{
  mat4 model;
  mat4 viewProj;
};

void main()
{
  v_color = a_color;
  gl_Position = viewProj * model * vec4(a_pos, 1.0);
}