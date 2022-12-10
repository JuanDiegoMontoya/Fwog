#version 460 core

layout(binding = 0) uniform sampler2D s_gAlbedo;
layout(binding = 1) uniform sampler2D s_gNormal;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_rsmIndirect;
layout(binding = 4) uniform sampler2D s_rsmDepthShadow;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec3 o_color;

layout(binding = 0, std140) uniform GlobalUniforms
{
  mat4 viewProj;
  mat4 invViewProj;
};

layout(binding = 1, std140) uniform ShadingUniforms
{
  mat4 sunViewProj;
  vec4 sunDir;
  vec4 sunStrength;
}shadingUniforms;

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

float Shadow(vec4 clip, vec3 normal, vec3 lightDir)
{
  vec2 uv = clip.xy * .5 + .5;
  if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
  {
    return 0;
  }

  float viewDepth = clip.z * .5 + .5;
  float lightDepth = textureLod(s_rsmDepthShadow, uv, 0).x;

  // Analytically compute slope-scaled bias
  const float maxBias = 0.0018;
  const float quantize = 2.0 / (1 << 23);
  ivec2 res = textureSize(s_rsmDepthShadow, 0);
  float b = 1.0 / max(res.x, res.y) / 2.0;
  float NoD = clamp(-dot(shadingUniforms.sunDir.xyz, normal), 0.0, 1.0);
  float bias = quantize + b * length(cross(-shadingUniforms.sunDir.xyz, normal)) / NoD;
  bias = min(bias, maxBias);

  lightDepth += bias;
  
  float lightOcclusion = 0.0;
  if (lightDepth >= viewDepth)
  {
    lightOcclusion += 1.0;
  }

  return lightOcclusion;
}

void main()
{
  vec3 albedo = textureLod(s_gAlbedo, v_uv, 0.0).rgb;
  vec3 normal = textureLod(s_gNormal, v_uv, 0.0).xyz;
  float depth = textureLod(s_gDepth, v_uv, 0.0).x;

  if (depth == 1.0)
  {
    o_color = albedo;
    return;
  }

  vec3 worldPos = UnprojectUV(depth, v_uv, invViewProj);
  
  vec3 incidentDir = -shadingUniforms.sunDir.xyz;
  float cosTheta = max(0.0, dot(incidentDir, normal));
  vec3 diffuse = albedo * cosTheta * shadingUniforms.sunStrength.rgb;
  diffuse *= Shadow(shadingUniforms.sunViewProj * vec4(worldPos, 1.0), normal, shadingUniforms.sunDir.xyz);

  //vec3 ambient = vec3(.03) * albedo;
  vec3 ambient = textureLod(s_rsmIndirect, v_uv, 0).rgb;
  vec3 finalColor = diffuse + ambient;

  // tone mapping (optional)
  //finalColor = finalColor / (1.0 + finalColor);
  o_color = finalColor;
}