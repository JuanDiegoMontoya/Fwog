#ifndef COMMON_H
#define COMMON_H

// unproject with zero-origin convention [0, 1]
vec3 UnprojectUVZO(float depth, vec2 uv, mat4 invXProj)
{
  vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0); // [0, 1] -> [-1, 1]

  vec4 worldSpacePosition = invXProj * clipSpacePosition;
  worldSpacePosition /= worldSpacePosition.w;

  return worldSpacePosition.xyz;
}

// unproject with GL convention [-1, 1]
vec3 UnprojectUVGL(float depth, vec2 uv, mat4 invXProj)
{
  depth = depth * 2.0 - 1.0; // [0, 1] -> [-1, 1]
  return UnprojectUVZO(depth, uv, invXProj);
}

float LinearizeDepthZO(float nonlinearZ, float zn, float zf)
{
  return zn / (zf + nonlinearZ * (zn - zf));
}

// the inverse of LinearizeDepthZO
float InvertDepthZO(float linearZ, float zn, float zf)
{
  return (zn - zf * linearZ) / (linearZ * (zn - zf));
}

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
  uint useScatteringTexture;
  float anisotropyG;
  float noiseOffsetScale;
  uint frog;
  float groundFogDensity;
  vec3 sunColor;
}uniforms;

#define M_PI 3.1415926

// Henyey-Greenstein phase function for anisotropic in-scattering
float phaseHG(float g, float cosTheta)
{
  return (1.0 - g * g) / (4.0 * M_PI * pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5));
}

// Schlick's efficient approximation of HG
float phaseSchlick(float k, float cosTheta)
{
  float denom = 1.0 - k * cosTheta;
  return (1.0 - k * k) / (4.0 * M_PI * denom * denom);
}

// Conversion of HG's G parameter to Schlick's K parameter
float gToK(float g)
{
  return clamp(1.55 * g - 0.55 * g * g * g, -0.999, 0.999);
}

// Beer-Lambert law
float beer(float d)
{
  return exp(-d);
}

// Powder scattering effect for large volumes (darkens edges, used with beer)
float powder(float d)
{
  return 1.0 - exp(-d * 2.0);
}

#endif