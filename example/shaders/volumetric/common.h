#ifndef COMMON_H
#define COMMON_H

// unproject with zero-origin convention [0, 1]
vec3 UnprojectUVZO(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth;
  vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0); // [0, 1] -> [-1, 1]

  vec4 worldSpacePosition = invXProj * clipSpacePosition;
  worldSpacePosition /= worldSpacePosition.w;

  return worldSpacePosition.xyz;
}

// unproject with GL convention [-1, 1]
vec3 UnprojectUVGL(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // [0, 1] -> [-1, 1]
  vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0); // [0, 1] -> [-1, 1]

  vec4 worldSpacePosition = invXProj * clipSpacePosition;
  worldSpacePosition /= worldSpacePosition.w;

  return worldSpacePosition.xyz;
}

float LinearizeDepthZO(float d, float zn, float zf)
{
  return zn / (zf + d * (zn - zf));
}

// the inverse of LinearizeDepthZO
float InvertDepthZO(float l, float zn, float zf)
{
  return (zn - zf * l) / (l * (zn - zf));
}

#endif