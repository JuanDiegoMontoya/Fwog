#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "common.h"

layout(binding = 0) uniform sampler3D s_colorDensityVolume;
layout(binding = 1) uniform sampler2D s_exponentialShadowDepth;
layout(binding = 0) uniform writeonly image3D i_inScatteringTransmittanceVolume;

layout(binding = 0, std140) uniform UNIFORMS 
{
  vec3 viewPos;
  float time;
  mat4 invViewProjScene;
  mat4 viewProjVolume;
  mat4 invViewProjVolume;
  mat4 sunViewProj;
  vec3 sunDir;
  float volumeNearPlane;
  float volumeFarPlane;
}uniforms;

layout(binding = 1, std140) uniform ESM_UNIFORMS
{
  float depthExponent;
}esmUniforms;

struct Light
{
  vec4 position;
  vec3 intensity;
  float invRadius;
};

layout(binding = 0, std430) readonly buffer LightBuffer
{
  Light lights[];
}lightBuffer;

#define PI 3.1415926

// henyey-greenstein phase function for isotropic in-scattering
float phaseHG(float g, float cosTheta)
{
  return (1.0 - g * g) / (4.0 * PI * pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5));
}

float phaseSchlick(float k, float cosTheta)
{
  float denom = 1.0 - k * cosTheta;
  return (1.0 - k * k) / (4.0 * PI * denom * denom);
}

float gToK(float g)
{
  return 1.55 * g - 0.55 * g * g * g;
}

float beer(float d)
{
  return exp(-d);
}

float powder(float d)
{
  return 1.0 - exp(-d * 2.0);
}

float ShadowESM(vec4 clip)
{
  vec4 unorm = clip;
  unorm.xy = unorm.xy * .5 + .5;
  float lightDepth = textureLod(s_exponentialShadowDepth, unorm.xy, 0.0).x;
  float eyeDepth = unorm.z;
  return clamp(lightDepth * exp(-esmUniforms.depthExponent * eyeDepth), 0.0, 1.0);
}

float GetSquareFalloffAttenuation(vec3 posToLight, float lightInvRadius)
{
  float distanceSquared = dot(posToLight, posToLight);
  float factor = distanceSquared * lightInvRadius * lightInvRadius;
  float smoothFactor = max(1.0 - factor * factor, 0.0);
  return (smoothFactor * smoothFactor) / max(distanceSquared, 1e-4);
}

vec3 LocalLightIntensity(vec3 wPos, vec3 V, float b, float p, float d, vec4 s, float k)
{
  vec3 color = { 0, 0, 0 };

  for (int i = 0; i < lightBuffer.lights.length(); i++)
  {
    Light light = lightBuffer.lights[i];
    
    //if (distance(wPos, light.position.xyz) < 2)
      //color += 3.0;
    //continue;
    vec3 diffuse = s.rgb * light.intensity;

    vec3 localColor = diffuse;
    localColor *= GetSquareFalloffAttenuation(light.position.xyz - wPos, light.invRadius);

    vec3 L = normalize(light.position.xyz - wPos);
    localColor *= phaseSchlick(k, dot(-V, L));

    color += localColor;
  }

  return color * d * b * p * s.a;
}

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec3 targetDim = imageSize(i_inScatteringTransmittanceVolume);
  if (any(greaterThanEqual(gid, targetDim.xy)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim.xy;

  vec3 texel = 1.0 / targetDim;

  vec3 inScatteringAccum = vec3(0.0);
  float densityAccum = 0.0;
  vec3 pPrev = uniforms.viewPos;
  for (int i = 0; i < targetDim.z; i++)
  {
    // uvw is the current voxel in unorm (UV) space. One half is added to i to get the center of the voxel as usual.
    vec3 uvw = vec3(uv, (i + 0.5) * texel.z);
    
    // Square the depth to bias precision towards the viewer,
    // then 
    float zInv = InvertDepthZO(uvw.z, uniforms.volumeNearPlane, uniforms.volumeFarPlane);
    vec3 pCur = UnprojectUVZO(zInv, uv, uniforms.invViewProjVolume);
    float d = distance(pPrev, pCur);
    pPrev = pCur;

    vec3 viewDir = normalize(pCur - uniforms.viewPos);
    float g = 0.2;
    float k = gToK(g);

    vec4 s = textureLod(s_colorDensityVolume, uvw, 0);
    densityAccum += s.a * d;
    float b = beer(densityAccum);
    float p = powder(densityAccum);
    inScatteringAccum += s.rgb * d * b * p * s.a * phaseSchlick(k, dot(-viewDir, uniforms.sunDir)) * ShadowESM(uniforms.sunViewProj * vec4(pCur, 1.0));
    inScatteringAccum += LocalLightIntensity(pCur, viewDir, b, p, d, s, k);
    float transmittance = b;
    imageStore(i_inScatteringTransmittanceVolume, ivec3(gid, i), vec4(inScatteringAccum, transmittance));
  }
}