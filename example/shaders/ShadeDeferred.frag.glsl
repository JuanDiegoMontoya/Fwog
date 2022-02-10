#version 460 core

layout(binding = 0) uniform sampler2D s_gAlbedo;
layout(binding = 1) uniform sampler2D s_gNormal;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_rsmFlux;
layout(binding = 4) uniform sampler2D s_rsmNormal;
layout(binding = 5) uniform sampler2D s_rsmDepth;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 o_color;

layout(binding = 0, std140) uniform UBO0
{
  mat4 viewProj;
  mat4 invViewProj;
};

layout(binding = 1, std140) uniform UBO1
{
  vec4 viewPos;
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

vec3 Shadow(vec4 clip)
{
  vec3 ndc = clip.xyz / clip.w;
  ndc = ndc * .5 + .5;
  float shadowDepth = texture(s_rsmDepth, ndc.xy).x;
  if (shadowDepth < ndc.z) return 0.0.rrr;
  return 1.0.rrr;
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}

vec3 ComputePixelLight()
{
  return vec3(0,0,0);
}

vec3 ComputeIndirectIrradiance(vec3 albedo, vec3 normal, vec3 worldPos)
{
  vec3 sumC = { 0, 0, 0 };

  return vec3(.03) * albedo;
}

void main()
{
  vec3 albedo = texture(s_gAlbedo, v_uv).rgb;
  vec3 normal = texture(s_gNormal, v_uv).xyz;
  float depth = texture(s_gDepth, v_uv).x;
  vec3 worldPos = UnprojectUV(depth, v_uv, invViewProj);
  
  vec3 incidentDir = -shadingUniforms.sunDir.xyz;
  float cosTheta = max(0.0, dot(incidentDir, normal));
  vec3 diffuse = albedo * cosTheta * shadingUniforms.sunStrength.rgb;
  diffuse *= Shadow(shadingUniforms.sunViewProj * vec4(worldPos, 1.0));

  vec3 ambient = ComputeIndirectIrradiance(albedo, normal, worldPos);
  vec3 finalColor = diffuse + ambient;

  o_color = vec4(finalColor, 1.0);

  if (depth == 1.0)
  {
    o_color.rgb = vec3(.1, .3, .5);
  }
}