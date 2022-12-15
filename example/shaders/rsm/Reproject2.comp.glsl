#version 460 core
#extension GL_GOOGLE_include_directive : enable

#define DEBUG 0

#define KERNEL_3x3
#include "Kernels.h.glsl"
#include "Common.h.glsl"

layout(binding = 0) uniform sampler2D s_indirectCurrent;
layout(binding = 1) uniform sampler2D s_indirectPrevious;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_gDepthPrev;
layout(binding = 4) uniform sampler2D s_gNormal;
layout(binding = 5) uniform sampler2D s_gNormalPrev;
layout(binding = 6) uniform sampler2D s_momentsPrev;

layout(binding = 0) uniform restrict writeonly image2D i_outIndirect;
layout(binding = 1) uniform restrict writeonly image2D i_outMoments;
layout(binding = 2, r8ui) uniform restrict uimage2D i_historyLength;

layout(binding = 0, std140) uniform ReprojectionUniforms
{
  mat4 invViewProjCurrent;
  mat4 viewProjPrevious;
  mat4 invViewProjPrevious;
  mat4 proj;
  vec3 viewPos;
  float temporalWeightFactor;
  ivec2 targetDim;
  float alphaIlluminance;
  float alphaMoments;
  float phiDepth;
  float phiNormal;
}uniforms;

bool InBounds(ivec2 pos)
{
  return all(lessThan(pos, uniforms.targetDim)) && all(greaterThanEqual(pos, ivec2(0)));
}

void Accumulate(vec3 prevColor, vec3 curColor, vec2 prevMoments, vec2 curMoments, ivec2 gid)
{
  uint historyLength = min(1 + imageLoad(i_historyLength, gid).x, 255);
  imageStore(i_historyLength, gid, uvec4(historyLength));
  float alphaIlluminance = max(uniforms.alphaIlluminance, 1.0 / historyLength);
  float alphaMoments = max(uniforms.alphaMoments, 1.0 / historyLength);

  vec3 outColor = mix(prevColor, curColor, alphaIlluminance);
  imageStore(i_outIndirect, gid, vec4(outColor, 0.0));
  vec2 outMoments = mix(prevMoments, curMoments, alphaMoments);
  imageStore(i_outMoments, gid, vec4(outMoments, 0.0, 0.0));
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  if (!InBounds(gid))
  {
    return;
  }

  vec2 uv = (vec2(gid) + 0.5) / uniforms.targetDim;

  // Reproject this pixel
  float depthCur = texelFetch(s_gDepth, gid, 0).x;
  // NDC_curFrame -> world
  vec3 worldPosCur = UnprojectUV(depthCur, uv, uniforms.invViewProjCurrent);
  // world -> NDC_prevFrame
  vec4 clipPosPrev = uniforms.viewProjPrevious * vec4(worldPosCur, 1.0);
  vec3 ndcPosPrev = clipPosPrev.xyz / clipPosPrev.w;
  vec3 reprojectedUV = ndcPosPrev;
  reprojectedUV.xy = ndcPosPrev.xy * .5 + .5;
  // From OpenGL Z convention [-1, 1] -> [0, 1].
  // In other APIs (or with glClipControl(..., GL_ZERO_TO_ONE)) you would not do this.
  reprojectedUV.z = ndcPosPrev.z * .5 + .5;
  
  //ivec2 centerPos = ivec2(reprojectedUV.xy * uniforms.targetDim);

  vec3 rayDir = normalize(worldPosCur - uniforms.viewPos);

  vec3 normalCur = texelFetch(s_gNormal, gid, 0).xyz;

  // Locate consistent samples to interpolate between in 2x2 area
  ivec2 bottomLeftPos = ivec2(reprojectedUV.xy * uniforms.targetDim - 0.5);
  vec3 colors[2][2] = vec3[2][2](vec3[2](vec3(0), vec3(0)), vec3[2](vec3(0), vec3(0)));
  vec2 moments[2][2] = vec2[2][2](vec2[2](vec2(0), vec2(0)), vec2[2](vec2(0), vec2(0)));
  float valid[2][2] = float[2][2](float[2](0, 0), float[2](0, 0));
  int validCount = 0;
  for (int y = 0; y <= 1; y++)
  {
    for (int x = 0; x <= 1; x++)
    {
      ivec2 pos = bottomLeftPos + ivec2(x, y);
      if (!InBounds(pos))
      {
        continue;
      }

      float depthPrev = texelFetch(s_gDepthPrev, pos, 0).x;
      if (DepthWeight(depthPrev, depthCur, normalCur, rayDir, uniforms.proj, uniforms.phiDepth) < 0.75)
      {
        continue;
      }

      vec3 normalPrev = texelFetch(s_gNormalPrev, pos, 0).xyz;
      if (NormalWeight(normalPrev, normalCur, uniforms.phiNormal) < 0.75)
      {
        continue;
      }

      validCount++;
      valid[x][y] = 1.0;
      colors[x][y] = texelFetch(s_indirectPrevious, pos, 0).rgb;
      moments[x][y] = texelFetch(s_momentsPrev, pos, 0).xy;
    }
  }

  vec2 weight = fract(reprojectedUV.xy * uniforms.targetDim - 0.5);
  vec3 curColor = texelFetch(s_indirectCurrent, gid, 0).rgb;
  float lum = Luminance(curColor);
  vec2 curMoments = { lum, lum * lum };

  if (validCount > 0)
  {
    // Use weighted bilinear filter if any of its samples 
    float factor = max(0.01, Bilerp(valid[0][0], valid[0][1], valid[1][0], valid[1][1], weight));
    vec3 prevColor = Bilerp(colors[0][0], colors[0][1], colors[1][0], colors[1][1], weight) / factor;
    vec2 prevMoments = Bilerp(moments[0][0], moments[0][1], moments[1][0], moments[1][1], weight) / factor;

    Accumulate(prevColor, curColor, prevMoments, curMoments, gid);
  }
  else
  {
    // Search for valid samples in a 3x3 area with a bilateral filter
    ivec2 centerPos = ivec2(reprojectedUV.xy * uniforms.targetDim);
    vec3 accumIlluminance = vec3(0);
    vec2 accumMoments = vec2(0);
    float accumWeight = 0;

    for (int col = 0; col < kWidth; col++)
    {
      for (int row = 0; row < kWidth; row++)
      {
        ivec2 offset = ivec2(row - kRadius, col - kRadius);
        ivec2 pos = centerPos + offset;
        
        if (!InBounds(pos))
        {
          continue;
        }

        float kernelWeight = kernel[row][col];

        vec3 oColor = texelFetch(s_indirectPrevious, pos, 0).rgb;
        vec2 oMoments = texelFetch(s_momentsPrev, pos, 0).xy;
        vec3 oNormal = texelFetch(s_gNormalPrev, pos, 0).xyz;
        float oDepth = texelFetch(s_gDepthPrev, pos, 0).x;
        float phiDepth = offset == ivec2(0) ? 1.0 : length(vec2(offset));
        phiDepth *= uniforms.phiDepth;

        float normalWeight = NormalWeight(oNormal, normalCur, uniforms.phiNormal);
        float depthWeight = DepthWeight(oDepth, depthCur, normalCur, rayDir, uniforms.proj, phiDepth);
        
        float weight = normalWeight * depthWeight;
        accumIlluminance += oColor * weight * kernelWeight;
        accumMoments += oMoments * weight * kernelWeight;
        accumWeight += weight * kernelWeight;
      }
    }

    if (accumWeight >= 0.15)
    {
      // Consider bilateral filter a success if accumulated weight is above a threshold
      vec3 prevColor = accumIlluminance / accumWeight;
      vec2 prevMoments = accumMoments / accumWeight;
      Accumulate(prevColor, curColor, prevMoments, curMoments, gid);
    }
    else
    {
      // Disocclusion ocurred
      imageStore(i_outIndirect, gid, vec4(curColor, 0.0));
      imageStore(i_outMoments, gid, vec4(curMoments, 0.0, 0.0));
      imageStore(i_historyLength, gid, uvec4(0));

  #if DEBUG
      imageStore(i_outIndirect, gid, vec4(1, 0, 0, 0));
      return;
  #endif
    }
  }
}