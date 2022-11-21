#version 460 core

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
  mat4 proj; // The near & far planes are assumed to not change between frames
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
  return proj[3][2] / (proj[2][2] + (depth * 2.0 - 1.0));
}

float LinearizeDepth(float depth, mat4 proj)
{
  float near = GetViewDepth(0, proj);
  float far = GetViewDepth(1, proj);
  float actual = GetViewDepth(depth, proj);
  return (actual - near) / (far - near); // Map from [near, far] to [0, 1]
}

float RejectDepth(vec3 reprojectedUV)
{
  const vec2 texel = 1.0 / uniforms.targetDim;
  vec3 reprojectedWorldPos = UnprojectUV(reprojectedUV.z, reprojectedUV.xy, uniforms.invViewProjPrevious);

  vec2 bottomLeft = floor(reprojectedUV.xy * uniforms.targetDim) * texel;
  // vec4 depthPrevs = textureGather(s_gDepthPrev, reprojectedUV.xy, 0);
  vec4 depthPrevs;
  depthPrevs[0] = textureLodOffset(s_gDepthPrev, bottomLeft, 0, ivec2(0, 0)).x;
  depthPrevs[1] = textureLodOffset(s_gDepthPrev, bottomLeft, 0, ivec2(1, 0)).x;
  depthPrevs[2] = textureLodOffset(s_gDepthPrev, bottomLeft, 0, ivec2(0, 1)).x;
  depthPrevs[3] = textureLodOffset(s_gDepthPrev, bottomLeft, 0, ivec2(1, 1)).x;

  vec3 pixelPrevWorldPoses[4] =
    vec3[4](UnprojectUV(depthPrevs[0], bottomLeft + vec2(0, 0) * texel, uniforms.invViewProjPrevious),
            UnprojectUV(depthPrevs[1], bottomLeft + vec2(1, 0) * texel, uniforms.invViewProjPrevious),
            UnprojectUV(depthPrevs[2], bottomLeft + vec2(0, 1) * texel, uniforms.invViewProjPrevious),
            UnprojectUV(depthPrevs[3], bottomLeft + vec2(1, 1) * texel, uniforms.invViewProjPrevious));

  vec2 lerpFactor = (reprojectedUV.xy - bottomLeft) * uniforms.targetDim;
  vec3 bottomHLerp = mix(pixelPrevWorldPoses[0], pixelPrevWorldPoses[1], lerpFactor.x);
  vec3 topHLerp = mix(pixelPrevWorldPoses[2], pixelPrevWorldPoses[3], lerpFactor.x);
  vec3 finalLerp = mix(bottomHLerp, topHLerp, lerpFactor.y); // Approximate world position

  vec3 diff = finalLerp - reprojectedWorldPos;
  return exp(-dot(diff, diff) / .2);
}

float RejectDepthBad(vec3 reprojectedUV)
{
  vec3 reprojectedWorldPos = UnprojectUV(reprojectedUV.z, reprojectedUV.xy, uniforms.invViewProjPrevious);

  float depthPrev = textureLod(s_gDepthPrev, reprojectedUV.xy, 0).x;
  vec3 pixelPrevWorldPos = UnprojectUV(depthPrev, reprojectedUV.xy, uniforms.invViewProjPrevious);
  if (reprojectedUV.z > depthPrev && distance(reprojectedWorldPos, pixelPrevWorldPos) > .01)
  {
    return 0;
  }
  return 1;
}

float RejectDepth3(vec3 reprojectedUV)
{
#if 1
  // Choose the pixel in the reprojected 2x2 neighborhood with the closest depth to this one.
  vec4 depthPrevs = textureGather(s_gDepthPrev, reprojectedUV.xy, 0);
  vec4 diffs = abs(depthPrevs - reprojectedUV.z);
  float depthPrev;
  if (diffs[0] < diffs[1] && diffs[0] < diffs[2] && diffs[0] < diffs[3])
    depthPrev = depthPrevs[0];
  else if (diffs[1] < diffs[2] && diffs[1] < diffs[3])
    depthPrev = depthPrevs[1];
  else if (diffs[2] < diffs[3])
    depthPrev = depthPrevs[2];
  else
    depthPrev = depthPrevs[3];

  float linearDepthPrev = LinearizeDepth(depthPrev, uniforms.proj);
#else
  // Bilinearly interpolate linear depths
  const vec2 texel = 1.0 / uniforms.targetDim;
  //vec2 bottomLeft = floor((reprojectedUV.xy - texel * 0.5) * uniforms.targetDim) * texel;
  vec4 depthPrevs = textureGather(s_gDepthPrev, reprojectedUV.xy, 0);
  for (int i = 0; i < 4; i++) depthPrevs[i] = LinearizeDepth(depthPrevs[i], uniforms.proj);

  vec2 lerpFactor = fract((reprojectedUV.xy - texel * 0.5) * uniforms.targetDim);
  float topHLerp = mix(depthPrevs[0], depthPrevs[1], lerpFactor.x);
  float bottomHLerp = mix(depthPrevs[3], depthPrevs[2], lerpFactor.x);
  float linearDepthPrev = mix(bottomHLerp, topHLerp, lerpFactor.y);
#endif

  //float linearDepthPrev = LinearizeDepth(textureLod(s_gDepthPrev, reprojectedUV.xy, 0).x, uniforms.proj);
  
  float linearDepth = LinearizeDepth(reprojectedUV.z, uniforms.proj);
  
  // Some jank I cooked up with the help of a calculator
  // https://www.desmos.com/calculator/6qb6expmgq
  vec3 normalPrev = textureLod(s_gNormalPrev, reprojectedUV.xy, 0).xyz;

  float angleFactor = max(0, -dot(normalPrev, uniforms.viewDir));

  const float cutoff = 0.01;
  float baseFactor = (abs(linearDepth - linearDepthPrev) * angleFactor) / cutoff;
  float weight = 1.0 - baseFactor * baseFactor;

  return weight;
}

float RejectNormal(vec2 uv, vec3 reprojectedUV)
{
  vec3 normalCur = textureLod(s_gNormal, uv, 0).xyz;
  vec3 normalPrev = textureLod(s_gNormalPrev, reprojectedUV.xy, 0).xyz;

  float d = max(0, dot(normalCur, normalPrev));
  return d * d;
}

float RejectNormal2(vec2 uv, vec3 reprojectedUV)
{
  vec4 normalPrevsX = textureGather(s_gNormalPrev, reprojectedUV.xy, 0);
  vec4 normalPrevsY = textureGather(s_gNormalPrev, reprojectedUV.xy, 1);
  vec4 normalPrevsZ = textureGather(s_gNormalPrev, reprojectedUV.xy, 2);
  vec3 normals[4] = vec3[4](
    vec3(normalPrevsX[0], normalPrevsY[0], normalPrevsZ[0]),
    vec3(normalPrevsX[1], normalPrevsY[1], normalPrevsZ[1]),
    vec3(normalPrevsX[2], normalPrevsY[2], normalPrevsZ[2]),
    vec3(normalPrevsX[3], normalPrevsY[3], normalPrevsZ[3])
  );

  vec3 normalCur = textureLod(s_gNormal, uv, 0).xyz;

  vec4 dots = vec4(
    dot(normalCur, normals[0]), 
    dot(normalCur, normals[1]), 
    dot(normalCur, normals[2]), 
    dot(normalCur, normals[3]));
  vec3 normalPrev;
  if (dots[0] > dots[1] && dots[0] > dots[2] && dots[0] > dots[3])
    normalPrev = normals[0];
  else if (dots[1] > dots[2] && dots[1] > dots[3])
    normalPrev = normals[1];
  else if (dots[2] > dots[3])
    normalPrev = normals[2];
  else
    normalPrev = normals[3];

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

  float confidence = 1.0;

  // Reject previous sample if it is outside of the screen
  if (any(greaterThan(reprojectedUV, vec3(1))) || any(lessThan(reprojectedUV, vec3(0))))
  {
    confidence = 0;
//    imageStore(i_outIndirect, gid, vec4(0, 0, 1, 0.0));
//    return;
  }

  // Reject previous sample if this pixel was disoccluded
  //confidence *= RejectDepth(reprojectedUV);
  confidence *= RejectDepth3(reprojectedUV);
//  if (confidence <= .1)
//  {
//    imageStore(i_outIndirect, gid, vec4(1, 0, 0, 0.0));
//    return;
//  }

  // Reject previous sample if its normal is too different
  confidence *= RejectNormal2(uv, reprojectedUV);
//  if (confidence <= .1)
//  {
//    imageStore(i_outIndirect, gid, vec4(0, 1, 0, 0.0));
//    return;
//  }

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