#version 460 core
#extension GL_GOOGLE_include_directive : enable

#define KERNEL_5x5
#include "Kernels.h.glsl"
#include "Common.h.glsl"

layout(binding = 0) uniform sampler2D s_illuminance;
layout(binding = 1) uniform usampler2D s_historyLength;

layout(binding = 0) uniform writeonly image2D i_outMoments;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  const ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  //const int radius = 2; // 5x5
  const ivec2 targetDim = textureSize(s_illuminance, 0);

  if (any(lessThan(gid, ivec2(0))) || any(greaterThanEqual(gid, targetDim)))
  {
    return;
  }

  // We can rely on temporal integration with sufficient history
  uint historyLen = texelFetch(s_historyLength, gid, 0).x;
  if (historyLen >= 4)
  {
    return;
  }

  float mean = 0;
  float meanSquared = 0;

  float accumSamples = 0.01;

  for (uint y = 0; y < kWidth; y++)
  {
    for (uint x = 0; x < kWidth; x++)
    {
      ivec2 pos = gid + ivec2(x - kRadius, y - kRadius);
      if (any(greaterThanEqual(pos, targetDim)) || any(lessThan(pos, ivec2(0))))
      {
        continue;
      }

      float weight = kernel[x][y];
      vec3 c = texelFetch(s_illuminance, pos, 0).rgb;
      float lum = Luminance(c);
      accumSamples += weight;
      mean += lum * weight;
      meanSquared += lum * lum * weight;
    }
  }

  mean /= accumSamples;
  meanSquared /= accumSamples;
  imageStore(i_outMoments, gid, vec4(mean, meanSquared, 0.0, 0.0));
}