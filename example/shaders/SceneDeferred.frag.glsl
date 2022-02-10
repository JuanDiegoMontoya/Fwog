#version 460 core

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec3 o_normal;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

void main()
{
  o_color = vec3(v_uv, 0);
  o_normal = v_normal;
}