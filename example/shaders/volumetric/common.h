#ifndef COMMON_H
#define COMMON_H

#define DEPTH_ZERO_TO_ONE

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
#ifndef DEPTH_ZERO_TO_ONE
  float z = depth * 2.0 - 1.0; // [0, 1] -> [-1, 1]
#else
  float z = depth;
#endif
  vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0); // [0, 1] -> [-1, 1]

  // undo view + projection
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