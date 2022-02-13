#version 460 core

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec3 o_normal;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in vec3 v_color;

void main()
{
  o_color = v_color;
  o_normal = normalize(v_normal);
}