#version 460 core

#define TWO_PI (2.0 * 3.14159265)

layout(binding = 0) uniform sampler2D s_gAlbedo;
layout(binding = 1) uniform sampler2D s_gNormal;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_rsmFlux;
layout(binding = 4) uniform sampler2D s_rsmNormal;
layout(binding = 5) uniform sampler2D s_rsmDepth;
layout(binding = 6) uniform sampler2DShadow s_rsmDepthShadow;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec3 o_color;

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
  float rMax;
}shadingUniforms;

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}

float Shadow(vec4 clip)
{
  vec3 ndc = clip.xyz / clip.w;
  ndc = ndc * .5 + .5;

  float shadowy = 0.0;

  const int SHADOW_SAMPLES = 1;
  for (int i = 0; i < SHADOW_SAMPLES; i++)
  {
    vec2 xi = Hammersley(i, SHADOW_SAMPLES);
    float r = sqrt(xi.x) * .003;
    float theta = xi.y * TWO_PI;
    shadowy += texture(s_rsmDepthShadow, ndc + vec3(r * cos(theta), r * sin(theta), -0.0));
  }

  return shadowy / SHADOW_SAMPLES;
}

vec3 ComputePixelLight(vec3 surfaceWorldPos, vec3 surfaceNormal, vec3 rsmFlux, vec3 rsmWorldPos, vec3 rsmNormal)
{
  // move rsmPos in negative of normal by small constant amount(?)
  //rsmWorldPos -= rsmNormal * .01;
  float geometry = max(0.0, dot(rsmNormal, surfaceWorldPos - rsmWorldPos)) * 
                   max(0.0, dot(surfaceNormal, rsmWorldPos - surfaceWorldPos));
  float d = distance(surfaceWorldPos, rsmWorldPos);
  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 albedo, vec3 normal, vec3 worldPos)
{
  vec3 sumC = { 0, 0, 0 };

  const vec4 rsmClip = shadingUniforms.sunViewProj * vec4(worldPos, 1.0);
  const vec2 rsmUV = (rsmClip.xy / rsmClip.w) * .5 + .5;

  const int SAMPLES = 600;
  for (int i = 0; i < SAMPLES; i++)
  {
    vec2 xi = Hammersley(i, SAMPLES);
    float r = xi.x * shadingUniforms.rMax;
    float theta = xi.y * TWO_PI;
    vec2 pixelLightUV = rsmUV + vec2(r * cos(theta), r * sin(theta));

    vec3 rsmFlux = textureLod(s_rsmFlux, pixelLightUV, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, pixelLightUV, 0.0).xyz;
    float rsmDepth = textureLod(s_rsmDepth, pixelLightUV, 0.0).x;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, pixelLightUV, inverse(shadingUniforms.sunViewProj));

    sumC += ComputePixelLight(worldPos, normal, rsmFlux, rsmWorldPos, rsmNormal) * (xi.x * xi.x);
  }
  
  return sumC * albedo / SAMPLES;
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
  diffuse *= Shadow(shadingUniforms.sunViewProj * vec4(worldPos, 1.0));

  vec3 ambient = ComputeIndirectIrradiance(albedo, normal, worldPos);
  vec3 finalColor = diffuse + ambient;

  o_color = finalColor;
}