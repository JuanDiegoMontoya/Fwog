#version 460 core
#extension GL_ARB_bindless_texture : enable

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in flat uint v_materialIdx;

layout(binding = 0, std140) uniform GlobalUniforms
{
  mat4 viewProj;
  mat4 invViewProj;
  vec4 cameraPos;
}globalUniforms;

#define HAS_BASE_COLOR_TEXTURE (1 << 0)

struct Material
{
  uint flags;
  float alphaCutoff;
  uvec2 baseColorTextureHandle;
  vec4 baseColorFactor;
};

layout(binding = 1, std430) readonly buffer MaterialUniforms
{
  Material materials[];
};

layout(location = 0) out vec4 o_color;

void main()
{
  Material material = materials[v_materialIdx];

  vec4 color = material.baseColorFactor.rgba;
  if ((material.flags & HAS_BASE_COLOR_TEXTURE) != 0)
  {
    sampler2D samp = sampler2D(material.baseColorTextureHandle);
    color *= texture(samp, v_uv).rgba;
  }
  
  if (color.a < material.alphaCutoff)
  {
    discard;
  }

  vec3 albedo = color.rgb;
  vec3 normal = normalize(v_normal);
  
  vec3 viewDir = normalize(globalUniforms.cameraPos.xyz - v_position);
  float VoN = max(0.0, dot(viewDir, normal));
  vec3 diffuse = albedo * VoN;

  vec3 ambient = vec3(.1) * albedo;
  vec3 finalColor = diffuse + ambient;

  o_color = vec4(finalColor, 1.0);
}