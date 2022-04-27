#version 460 core

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec3 o_normal;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

layout(binding = 0) uniform sampler2D s_baseColor;

#define HAS_BASE_COLOR_TEXTURE (1 << 0)
layout(binding = 2, std140) uniform MaterialUniforms
{
  uint flags;
  float alphaCutoff;
  uint pad01;
  uint pad02;
  vec4 baseColorFactor;
}u_material;

void main()
{
  vec4 color = u_material.baseColorFactor.rgba;
  if ((u_material.flags & HAS_BASE_COLOR_TEXTURE) != 0)
  {
    color *= texture(s_baseColor, v_uv).rgba;
  }
  
  if (color.a < u_material.alphaCutoff)
  {
    discard;
  }
  
  o_color = color.rgb;
  o_normal = normalize(v_normal);
}