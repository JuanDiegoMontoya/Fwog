#version 460 core
#define DEBUG 0

layout(binding = 0) uniform sampler2D s_indirectUnfilteredCurrent;
layout(binding = 1) uniform sampler2D s_indirectUnfilteredPrevious;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_gDepthPrev;
layout(binding = 4) uniform sampler2D s_gNormal;
layout(binding = 5) uniform sampler2D s_gNormalPrev;

layout(binding = 0) uniform restrict writeonly image2D i_outIndirect;
layout(binding = 1, r8ui) uniform restrict uimage2D i_historyLength;

layout(binding = 0, std140) uniform ReprojectionUniforms
{
  mat4 invViewProjCurrent;
  mat4 viewProjPrevious;
  mat4 invViewProjPrevious;
  mat4 proj;
  vec3 viewDir;
  float temporalWeightFactor;
  ivec2 targetDim;
}uniforms;

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

float GetViewDepth(float depth, mat4 proj)
{
  // Returns linear depth in [near, far]
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  return proj[3][2] / (proj[2][2] + z);
}

float RejectDepth4(vec3 reprojectedUV)
{
  float linearDepthPrev = GetViewDepth(textureLod(s_gDepthPrev, reprojectedUV.xy, 0).x, uniforms.proj);
  
  float linearDepth = GetViewDepth(reprojectedUV.z, uniforms.proj);
  
  // Some jank I cooked up with the help of a calculator
  // https://www.desmos.com/calculator/6qb6expmgq
  vec3 normalPrev = textureLod(s_gNormalPrev, reprojectedUV.xy, 0).xyz;

  vec3 fn = UnprojectUV(.4, reprojectedUV.xy, uniforms.invViewProjCurrent);
  vec3 fp = UnprojectUV(.6, reprojectedUV.xy, uniforms.invViewProjCurrent);
  vec3 testDir = normalize(fp - fn);
  float angleFactor = max(0, -dot(normalPrev, testDir));

  // Cutoff in view space, inversely scaled by angleFactor
  const float cutoff = 0.1;
  float baseFactor = (abs(linearDepth - linearDepthPrev) * angleFactor) / cutoff;
  float weight = max(0, 1.0 - baseFactor * baseFactor);

  return weight;
}

float RejectNormal1(vec2 uv, vec3 reprojectedUV)
{
  vec3 normalCur = textureLod(s_gNormal, uv, 0).xyz;
  vec3 normalPrev = normalize(textureLod(s_gNormalPrev, reprojectedUV.xy, 0).xyz);

  float d = max(0, dot(normalCur, normalPrev));
  return d * d;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(gid, uniforms.targetDim)))
    return;

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
 // imageStore(i_outIndirect, gid, vec4(reprojectedUV.z));
 // return;

  float confidence = 1.0;

  // Reject previous sample if it is outside of the screen
  if (any(greaterThan(reprojectedUV, vec3(1))) || any(lessThan(reprojectedUV, vec3(0))))
  {
    confidence = 0;
#if DEBUG
    imageStore(i_outIndirect, gid, vec4(0, 0, 1, 0.0));
    return;
#endif
  }

  // Reject previous sample if this pixel was disoccluded
  //confidence *= RejectDepth(reprojectedUV);
  confidence *= RejectDepth4(reprojectedUV);
#if DEBUG
  if (confidence <= .1)
  {
    imageStore(i_outIndirect, gid, vec4(1, 0, 0, 0.0));
    return;
  }
#endif

  // Reject previous sample if its normal is too different
  confidence *= RejectNormal1(uv, reprojectedUV);
#if DEBUG
  if (confidence <= .1)
  {
    imageStore(i_outIndirect, gid, vec4(0, 1, 0, 0.0));
    return;
  }
#endif

  uint historyLength = imageLoad(i_historyLength, gid).x;
  vec3 prevColor = textureLod(s_indirectUnfilteredPrevious, reprojectedUV.xy, 0).rgb;
  vec3 curColor = textureLod(s_indirectUnfilteredCurrent, uv, 0).rgb;

  float historyFactor = 1.0 - max(.01, exp(-float(historyLength) / 32.0));
  float weight = clamp(1.0 - confidence * historyFactor, 0.01, 1.0);
  vec3 blended = mix(prevColor, curColor, max(weight, uniforms.temporalWeightFactor));
  // blended = vec3(historyLength / 255.0);

  imageStore(i_outIndirect, gid, vec4(blended, 0.0));

  // Update history
  if (round(confidence) == 0) 
  {
    imageStore(i_historyLength, gid, uvec4(0));
  }
  else
  {
    imageStore(i_historyLength, gid, uvec4(min(1 + historyLength, 255)));
  }
}