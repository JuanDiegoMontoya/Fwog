#version 460 core

// Input
layout(binding = 0) uniform sampler2D s_diffuseIrradiance;
layout(binding = 1) uniform sampler2D s_gBufferNormal;
layout(binding = 2) uniform sampler2D s_gBufferDepth;

layout(binding = 0, std140) uniform FilterUniforms
{
  mat4 proj;
  mat4 invViewProj;
  vec3 viewDir;
  float stepWidth;
  ivec2 targetDim;
}uniforms;

// Output
layout(binding = 0) uniform writeonly restrict image2D i_target;

const uint kRadius = 1;
const uint kWidth = 1 + 2 * kRadius;
const float kernel1D[kWidth] = {0.27901, 0.44198, 0.27901};
const float kernel[kWidth][kWidth] = {
  {kernel1D[0] * kernel1D[0], kernel1D[0] * kernel1D[1], kernel1D[0] * kernel1D[2]},
  {kernel1D[1] * kernel1D[0], kernel1D[1] * kernel1D[1], kernel1D[1] * kernel1D[2]},
  {kernel1D[2] * kernel1D[0], kernel1D[2] * kernel1D[1], kernel1D[2] * kernel1D[2]},
};

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

void AddFilterContribution(inout vec3 accumDiffuseIndirect,
                           inout float accumWeight,
                           vec3 cColor,
                           vec3 cNormal,
                           float cDepth,
                           uint row,
                           uint col,
                           ivec2 kernelStep,
                           ivec2 gid)
{
  ivec2 offset = ivec2(col - kRadius, row - kRadius) * kernelStep;
  ivec2 id = gid + offset;
  
  if (any(greaterThanEqual(id, uniforms.targetDim)) || any(lessThan(id, ivec2(0))))
  {
    return;
  }

  vec3 oColor = texelFetch(s_diffuseIrradiance, id, 0).rgb;
  vec3 oNormal = texelFetch(s_gBufferNormal, id, 0).xyz;
  float oDepth = GetViewDepth(texelFetch(s_gBufferDepth, id, 0).x, uniforms.proj);

  float normalDist = max(0, dot(cNormal, oNormal));
  float normalWeight = normalDist * normalDist;

  // Some jank I cooked up with the help of a calculator
  // https://www.desmos.com/calculator/6qb6expmgq
  float angleFactor = max(0, -dot(oNormal, uniforms.viewDir));
  const float cutoff = 0.01;
  float baseFactor = (abs(cDepth - oDepth) * angleFactor) / cutoff;
  float depthWeight = max(0, 1.0 - baseFactor * baseFactor);

  float weight = normalWeight * depthWeight;
  vec3 ctmp = texelFetch(s_diffuseIrradiance, id, 0).rgb;
  accumDiffuseIndirect += ctmp * weight * kernel[row][col];
  accumWeight += weight * kernel[row][col];
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(gid, uniforms.targetDim)))
  {
    return;
  }

  // const int KERNEL_SIZE = 5;
  // const float kernel[KERNEL_SIZE] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};
  // const int offsets[KERNEL_SIZE] = {-2, -1, 0, 1, 2};

  vec3 cColor = texelFetch(s_diffuseIrradiance, gid, 0).rgb;
  vec3 cNormal = texelFetch(s_gBufferNormal, gid, 0).xyz;
  float cDepth = GetViewDepth(texelFetch(s_gBufferDepth, gid, 0).x, uniforms.proj);

  vec3 accumDiffuseIndirect = vec3(0);
  float accumWeight = 0;

  for (int col = 0; col < kWidth; col++)
  {
    for (int row = 0; row < kWidth; row++)
    {
      ivec2 kernelStep = ivec2(1, 1) * ivec2(uniforms.stepWidth);
      AddFilterContribution(accumDiffuseIndirect, accumWeight, cColor, cNormal, cDepth, row, col, kernelStep, gid);
    }
  }

  imageStore(i_target, gid, vec4(accumDiffuseIndirect / accumWeight, 0.0));
}