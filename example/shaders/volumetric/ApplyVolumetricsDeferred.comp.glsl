#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

#define EPSILON .0001

layout(binding = 0) uniform sampler2D s_color;
layout(binding = 1) uniform sampler2D s_depth;
layout(binding = 2) uniform sampler3D s_volume;
layout(binding = 3) uniform sampler2D s_blueNoise;
layout(binding = 0) uniform writeonly image2D i_target;

// Ported from here: https://gist.github.com/Fewes/59d2c831672040452aa77da6eaab2234
vec4 textureTricubic(sampler3D volume, vec3 coord)
{
  vec3 texSize = textureSize(volume, 0);

  vec3 coord_grid = coord * texSize - 0.5;
  vec3 index = floor(coord_grid);
  vec3 fraction = coord_grid - index;
  vec3 one_frac = 1.0 - fraction;

  vec3 w0 = 1.0 / 6.0 * one_frac*one_frac*one_frac;
  vec3 w1 = 2.0 / 3.0 - 0.5 * fraction*fraction*(2.0-fraction);
  vec3 w2 = 2.0 / 3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
  vec3 w3 = 1.0 / 6.0 * fraction*fraction*fraction;

  vec3 g0 = w0 + w1;
  vec3 g1 = w2 + w3;
  vec3 mult = 1.0 / texSize;
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);

  vec4 tex000 = textureLod(volume, h0, 0.0);
  vec4 tex100 = textureLod(volume, vec3(h1.x, h0.y, h0.z), 0.0);
  tex000 = mix(tex100, tex000, g0.x);

  vec4 tex010 = textureLod(volume, vec3(h0.x, h1.y, h0.z), 0.0);
  vec4 tex110 = textureLod(volume, vec3(h1.x, h1.y, h0.z), 0.0);
  tex010 = mix(tex110, tex010, g0.x);
  tex000 = mix(tex010, tex000, g0.y);

  vec4 tex001 = textureLod(volume, vec3(h0.x, h0.y, h1.z), 0.0);
  vec4 tex101 = textureLod(volume, vec3(h1.x, h0.y, h1.z), 0.0);
  tex001 = mix(tex101, tex001, g0.x);

  vec4 tex011 = textureLod(volume, vec3(h0.x, h1.y, h1.z), 0.0);
  vec4 tex111 = textureLod(volume, h1, 0.0);
  tex011 = mix(tex111, tex011, g0.x);
  tex001 = mix(tex011, tex001, g0.y);

  return mix(tex001, tex000, g0.z);
}

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec2 targetDim = imageSize(i_target);
  if (any(greaterThanEqual(gid, targetDim)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim;
  
  // Get Z-buffer depth and reconstruct world position.
  float zScr = texelFetch(s_depth, gid, 0).x;
  zScr = max(zScr, EPSILON); // prevent infinities
  vec3 pWorld = UnprojectUVZO(zScr, uv, uniforms.invViewProjScene);

  // World position to volume clip space.
  vec4 volumeClip = uniforms.viewProjVolume * vec4(pWorld, 1.0);

  // Volume clip to volume UV (perspective divide).
  vec3 volumeUV = volumeClip.xyz / volumeClip.w;
  volumeUV.xy = volumeUV.xy * 0.5 + 0.5;
  
  // Linearize the window-space depth, then invert the transform applied in accumulateDensity.comp.glsl (volumeUV.z^2).
  volumeUV.z = sqrt(LinearizeDepthZO(volumeUV.z, uniforms.volumeNearPlane, uniforms.volumeFarPlane));

  // Random UV offset of up to half a froxel.
  vec3 offset = uniforms.noiseOffsetScale * (texelFetch(s_blueNoise, gid % textureSize(s_blueNoise, 0).xy, 0).xyz - 0.5);
  volumeUV += offset / vec3(textureSize(s_volume, 0).xyz);

  vec3 baseColor = texelFetch(s_color, gid, 0).xyz;
  vec4 scatteringInfo = textureTricubic(s_volume, volumeUV);
  vec3 inScattering = scatteringInfo.rgb;
  float transmittance = scatteringInfo.a;

  vec3 finalColor = baseColor * transmittance + inScattering;

  // Draw lines on froxel borders.
//  if (any(lessThan(fract(uv * vec2(textureSize(s_volume, 0).xy)), vec2(0.1))))
//  {
//    finalColor = vec3(1);
//  }

  imageStore(i_target, gid, vec4(finalColor, 1.0));
}