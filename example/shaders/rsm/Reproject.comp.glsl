#version 460 core

layout(binding = 0) uniform sampler2D s_indirectUnfilteredCurrent;
layout(binding = 1) uniform sampler2D s_indirectUnfilteredPrevious;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_gDepthPrev;
// layout(binding = 3) uniform sampler2D s_gNormal;

layout(binding = 0) uniform restrict writeonly image2D i_outIndirect;

layout(binding = 0, std140) uniform ReprojectionUniforms
{
  mat4 invViewProjCurrent;
  mat4 viewProjPrevious;
  mat4 invViewProjPrevious;
  ivec2 targetDim;
  float temporalWeightFactor;
}
uniforms;

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
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

  float confidence = 0.99;
  vec3 debugColor = vec3(0);

  // Reject previous sample if it is outside of the screen
  if (any(greaterThan(reprojectedUV, vec3(1))) || any(lessThan(reprojectedUV, vec3(0))))
  {
    confidence = 0;
    // debugColor += vec3(1, 0, 0);
  }

  // Reject previous sample if this pixel was disoccluded
  float depthPrev = textureLod(s_gDepthPrev, reprojectedUV.xy, 0).x;
  vec3 reprojectedWorldPos = UnprojectUV(reprojectedUV.z, reprojectedUV.xy, uniforms.invViewProjPrevious);
  vec3 pixelPrevWorldPos = UnprojectUV(depthPrev, reprojectedUV.xy, uniforms.invViewProjPrevious);
  if (reprojectedUV.z > depthPrev && distance(reprojectedWorldPos, pixelPrevWorldPos) > .01)
  {
    confidence = 0;
    // debugColor += vec3(0, 0, 1);
  }

  vec3 prevColor = textureLod(s_indirectUnfilteredPrevious, reprojectedUV.xy, 0).rgb;
  vec3 curColor = textureLod(s_indirectUnfilteredCurrent, uv, 0).rgb;
  vec3 blended = mix(prevColor, curColor, 1.0 - confidence * (1.0 - uniforms.temporalWeightFactor));
  blended += debugColor;

  imageStore(i_outIndirect, gid, vec4(blended, 0.0));
}