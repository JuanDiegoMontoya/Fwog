#version 450 core
#extension GL_GOOGLE_include_directive : enable

#define KERNEL_3x3
#include "Kernels.h.glsl"
#include "Common.h.glsl"

// Input
layout(binding = 0) uniform sampler2D s_diffuseIrradiance;
layout(binding = 1) uniform sampler2D s_gAlbedo;
layout(binding = 2) uniform sampler2D s_gNormal;
layout(binding = 3) uniform sampler2D s_gDepth;
layout(binding = 4) uniform sampler2D s_gNormalSmall;
layout(binding = 5) uniform sampler2D s_gDepthSmall;

layout(binding = 0, std140) uniform FilterUniforms
{
  mat4 proj;
  mat4 invViewProj;
  vec3 viewPos;
  float stepWidth;
  ivec2 targetDim;
  ivec2 direction;
  float phiNormal;
  float phiDepth;
}uniforms;

// Output
layout(binding = 0) uniform writeonly restrict image2D i_modulatedIlluminance;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(gid, uniforms.targetDim)))
  {
    return;
  }

  ivec2 sourceDim = textureSize(s_diffuseIrradiance, 0);

  vec3 cNormal = texelFetch(s_gNormal, gid, 0).xyz;
  float cDepth = texelFetch(s_gDepth, gid, 0).x;

  vec2 uv = (vec2(gid) + 0.5) / uniforms.targetDim;
  vec3 point = UnprojectUV(0.1, uv, uniforms.invViewProj);
  vec3 rayDir = normalize(point - uniforms.viewPos);

  vec2 ratio = vec2(sourceDim) / uniforms.targetDim;
  ivec2 sourcePos = ivec2(gid * ratio);

  vec3 accumIlluminance = vec3(0);
  float accumWeight = 0;

  // Do a widdle 3x3 bilateral filter to find valid samples
  for (int col = 0; col < kWidth; col++)
  {
    for (int row = 0; row < kWidth; row++)
    {
      ivec2 offset = ivec2(row - kRadius, col - kRadius);
      ivec2 pos = sourcePos + offset;
      
      if (any(greaterThanEqual(pos, sourceDim)) || any(lessThan(pos, ivec2(0))))
      {
        continue;
      }

      float kernelWeight = kernel[row][col];

      vec3 oColor = texelFetch(s_diffuseIrradiance, pos, 0).rgb;
      vec3 oNormal = texelFetch(s_gNormalSmall, pos, 0).xyz;
      float oDepth = texelFetch(s_gDepthSmall, pos, 0).x;

      float normalWeight = NormalWeight(oNormal, cNormal, uniforms.phiNormal);
      float depthWeight = DepthWeight(oDepth, cDepth, cNormal, rayDir, uniforms.proj, uniforms.phiDepth);
      
      float weight = normalWeight * depthWeight;
      accumIlluminance += oColor * weight * kernelWeight;
      accumWeight += weight * kernelWeight;
    }
  }

  vec3 albedo = texelFetch(s_gAlbedo, gid, 0).rgb;
  if (accumWeight >= 0.0001)
  {
    imageStore(i_modulatedIlluminance, gid, vec4(albedo * accumIlluminance / accumWeight, 0.0));
  }
  else
  {
    vec3 center = texelFetch(s_diffuseIrradiance, sourcePos, 0).rgb;
    imageStore(i_modulatedIlluminance, gid, vec4(albedo * center, 0.0));
  }
}