#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "common.h"

#define EPSILON .00001

layout(binding = 0) uniform sampler2D s_color;
layout(binding = 1) uniform sampler2D s_depth;
layout(binding = 2) uniform sampler3D s_volume;
layout(binding = 3) uniform sampler2D s_blueNoise;
layout(binding = 0) uniform writeonly image2D i_target;

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

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec2 targetDim = imageSize(i_target);
  if (any(greaterThanEqual(gid, targetDim)))
  {
    return;
  }
  vec2 uv = (vec2(gid) + 0.5) / targetDim;

  float z = texelFetch(s_depth, gid, 0).x;
  z = max(z, EPSILON); // prevent infinities
  vec3 p = UnprojectUV(z, uv, uniforms.invViewProjScene);

  vec4 volumeClip = uniforms.viewProjVolume * vec4(p, 1.0);
  volumeClip.xyz = clamp(volumeClip.xyz, -volumeClip.www, volumeClip.www);
  vec3 volumeUV = volumeClip.xyz / volumeClip.w;
  volumeUV.z = LinearizeDepthZO(pow(volumeUV.z, 1.0 / 2.0), uniforms.volumeNearPlane, uniforms.volumeFarPlane);
  volumeUV.xy = (volumeUV.xy * 0.5) + 0.5;
  vec3 offset = texelFetch(s_blueNoise, gid % textureSize(s_blueNoise, 0).xy, 0).xyz * 2. - 1.;
  volumeUV += offset / vec3(textureSize(s_volume, 0).xyz);
  if (volumeClip.z / volumeClip.w > 1.0)
  {
    volumeUV.z = 1.0;
  }

  vec3 baseColor = texelFetch(s_color, gid, 0).xyz;
  vec4 scatteringInfo = textureLod(s_volume, volumeUV, 0.0);
  vec3 inScattering = scatteringInfo.rgb;
  float transmittance = scatteringInfo.a;

  vec3 finalColor = baseColor * transmittance + inScattering;
  imageStore(i_target, gid, vec4(finalColor, 1.0));
}