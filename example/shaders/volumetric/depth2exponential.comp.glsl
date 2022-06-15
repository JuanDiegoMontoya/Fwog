#version 450 core

layout(binding = 0) uniform sampler2D s_depth;
layout(binding = 0) uniform writeonly image2D i_depthExp;

layout(binding = 0, std140) uniform UNIFORMS
{
  float depthExponent;
}uniforms;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec2 targetDim = imageSize(i_depthExp);
  if (any(greaterThanEqual(gid, targetDim)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim.xy;

  float depth = textureLod(s_depth, uv, 0).x;
  float depthExp = exp(depth * uniforms.depthExponent);
  imageStore(i_depthExp, gid, depthExp.xxxx);
}