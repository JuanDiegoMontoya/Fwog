#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_GOOGLE_include_directive : enable

#include "Common.h"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in flat uint v_materialIdx;

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