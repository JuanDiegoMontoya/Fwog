#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

#define EPSILON .0001

layout(binding = 0) uniform sampler2D s_color;
layout(binding = 1) uniform sampler2D s_depth;
layout(binding = 2) uniform sampler3D s_volume;
layout(binding = 3) uniform sampler2D s_blueNoise;
layout(binding = 0) uniform writeonly image2D i_target;

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
  vec4 scatteringInfo = textureLod(s_volume, volumeUV, 0.0);
  vec3 inScattering = scatteringInfo.rgb;
  float transmittance = scatteringInfo.a;

  vec3 finalColor = baseColor * transmittance + inScattering;

  imageStore(i_target, gid, vec4(finalColor, 1.0));
}