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
  float isotropyG;
  float noiseOffsetScale;
  uint frog;
}uniforms;

#endif