#version 460 core

layout(binding = 0) uniform sampler2D s_image;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 o_color;

void main()
{
  o_color = vec4(texture(s_image, v_uv).rgb, 1.0);
}