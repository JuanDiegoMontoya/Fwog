#version 460 core

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec2 o_motion;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in vec3 v_color;
layout(location = 4) in vec4 v_curPos;
layout(location = 5) in vec4 v_oldPos;

void main()
{
  o_color = v_color;
  o_normal = normalize(v_normal);
  o_motion = ((v_oldPos.xy / v_oldPos.w) - (v_curPos.xy / v_curPos.w)) * 0.5;
}