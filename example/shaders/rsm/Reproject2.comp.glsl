#version 460 core
#define DEBUG 0

layout(binding = 0) uniform sampler2D s_indirectCurrent;
layout(binding = 1) uniform sampler2D s_indirectPrevious;
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

bool RejectDepth(float depthPrev, float depthCur, vec3 normalCur, float threshold)
{
  float linearDepthPrev = GetViewDepth(depthPrev, uniforms.proj);
  
  float linearDepth = GetViewDepth(depthCur, uniforms.proj);
  
  // Some jank I cooked up with the help of a calculator
  // https://www.desmos.com/calculator/6qb6expmgq

  float angleFactor = max(0, -dot(normalCur, uniforms.viewDir));

  // Cutoff in view space, inversely scaled by angleFactor
  const float cutoff = 0.1;
  float baseFactor = (abs(linearDepth - linearDepthPrev) * angleFactor) / cutoff;
  float weight = max(0, 1.0 - baseFactor * baseFactor);

  return weight > threshold;
}

bool RejectNormal(vec3 normalPrev, vec3 normalCur, float threshold)
{
  float d = max(0, dot(normalCur, normalPrev));
  return (d * d) > threshold;
}

bool InBounds(ivec2 pos)
{
  return all(lessThan(pos, uniforms.targetDim)) && all(greaterThanEqual(pos, ivec2(0)));
}

vec3 Bilerp(vec3 _00, vec3 _01, vec3 _10, vec3 _11, vec2 weight)
{
  vec3 bottom = mix(_00, _10, weight.x);
  vec3 top = mix(_01, _11, weight.x);
  return mix(bottom, top, weight.y);
}

float Bilerp(float _00, float _01, float _10, float _11, vec2 weight)
{
  float bottom = mix(_00, _10, weight.x);
  float top = mix(_01, _11, weight.x);
  return mix(bottom, top, weight.y);
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  if (!InBounds(gid))
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
  
  //ivec2 centerPos = ivec2(reprojectedUV.xy * uniforms.targetDim);

  vec3 normalCur = texelFetch(s_gNormal, gid, 0).xyz;

  // Locate consistent samples to interpolate between in 2x2 area
  ivec2 bottomLeftPos = ivec2(reprojectedUV.xy * uniforms.targetDim - (0.5 + 1.0 / 512.0));
  vec3 colors[2][2] = vec3[2][2](vec3[2](vec3(0), vec3(0)), vec3[2](vec3(0), vec3(0)));
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
      if (!RejectDepth(depthPrev, depthCur, normalCur, 0.5))
      {
        continue;
      }

      vec3 normalPrev = texelFetch(s_gNormalPrev, pos, 0).xyz;
      if (!RejectNormal(normalPrev, normalCur, 0.5))
      {
        continue;
      }

      validCount++;
      valid[x][y] = 1.0;
      colors[x][y] = texelFetch(s_indirectPrevious, pos, 0).rgb;
    }
  }

  vec2 weight = fract(reprojectedUV.xy * uniforms.targetDim - (0.5 + 1.0 / 512.0));
  vec3 prevColor;
  vec3 curColor = texelFetch(s_indirectCurrent, gid, 0).rgb;

  if (validCount > 0)
  {
    float factor = Bilerp(valid[0][0], valid[0][1], valid[1][0], valid[1][1], weight);
    prevColor = Bilerp(colors[0][0], colors[0][1], colors[1][0], colors[1][1], weight) / (factor + .0001);
    uint historyLength = imageLoad(i_historyLength, gid).x;
    imageStore(i_historyLength, gid, uvec4(min(1 + historyLength, 255)));
  } 
  else // Disocclusion ocurred
  {
    prevColor = curColor;
    imageStore(i_historyLength, gid, uvec4(0));

#if DEBUG
    imageStore(i_outIndirect, gid, vec4(1, 0, 0, 0));
    return;
#endif
  }

  vec3 outColor = mix(prevColor, curColor, 0.2);
  imageStore(i_outIndirect, gid, vec4(outColor, 0.0));
}