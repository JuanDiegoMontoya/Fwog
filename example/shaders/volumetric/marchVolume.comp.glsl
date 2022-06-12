#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "common.h"

layout(binding = 0) uniform sampler3D s_source;
layout(binding = 1) uniform sampler2DShadow s_shadowDepth;
layout(binding = 0) uniform writeonly image3D i_target;

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

float Shadow(vec4 clip)
{
  return textureProj(s_shadowDepth, clip * .5 + .5);
}

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec3 targetDim = imageSize(i_target);
  if (any(greaterThanEqual(gid, targetDim.xy)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim.xy;

  vec3 texel = 1.0 / targetDim;

  vec3 inScatteringAccum = vec3(0.0);
  float densityAccum = 0.0;
  vec3 pPrev = uniforms.viewPos;
  for (int i = 0; i < targetDim.z; i++)
  {
    vec3 uvw = vec3(uv, (i + 0.5) / targetDim.z);
    float zInv = InvertDepthZO(uvw.z * uvw.z, uniforms.volumeNearPlane, uniforms.volumeFarPlane);
    vec3 pCur = UnprojectUV(zInv, uv, uniforms.invViewProjVolume);
    float d = distance(pPrev, pCur);
    pPrev = pCur;

    vec3 viewDir = normalize(pCur - uniforms.viewPos);
    //vec3 sunDir = normalize(vec3(.2, -.25, -.15)); // hardcoded until procedural sky
    float g = 0.4;
    float k = gToK(g);

    vec4 s = textureLod(s_source, uvw, 0);
    densityAccum += s.a * d;
    float b = beer(densityAccum);
    float p = powder(densityAccum);
    inScatteringAccum += s.rgb * d * b * p * s.a * phaseSchlick(k, dot(-viewDir, normalize(uniforms.sunDir))) * Shadow(uniforms.sunViewProj * vec4(pCur, 1.0));
    float transmittance = b;
    imageStore(i_target, ivec3(gid, i), vec4(inScatteringAccum, transmittance));
  }
}