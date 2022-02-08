#version 460 core

layout(location = 0) out vec4 o_color;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

void main()
{
  o_color = vec4(v_uv, 0, 1.0);
  if (!gl_FrontFacing)
    o_color.rgb = vec3(1, 0, 0);
}