#version 460 core
#define KERNEL_RADIUS 3

layout(binding = 0) uniform sampler2D s_in;
layout(binding = 0) uniform writeonly restrict image2D i_out;

layout(binding = 0, std140) uniform UNIFORMS
{
  ivec2 direction;
  ivec2 targetDim;
}uniforms;

#if KERNEL_RADIUS == 6
const float weights[] = { 0.22528, 0.192187, 0.119319, 0.053904, 0.017716, 0.004235 }; // 11x11
#elif KERNEL_RADIUS == 5
const float weights[] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 }; // 9x9
#elif KERNEL_RADIUS == 4
const float weights[] = { 0.235624, 0.201012, 0.124798, 0.056379 }; // 7x7
#elif KERNEL_RADIUS == 3
const float weights[] = { 0.265569, 0.226558, 0.140658 }; // 5x5
#elif KERNEL_RADIUS == 2
const float weights[] = { 0.369521, 0.31524 }; // 3x3
#elif KERNEL_RADIUS == 1
const float weights[] = { 1.0 }; // 1x1 (lol)
#endif

layout (local_size_x = 8, local_size_y = 8) in;
void main()
{
  const ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(gid, uniforms.targetDim)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / uniforms.targetDim.xy;

  vec2 texel = 1.0 / uniforms.targetDim;

  vec4 color = textureLod(s_in, uv, 0).rgba * weights[0];

  for (int i = 1; i < KERNEL_RADIUS; i++)
  {
    color += textureLod(s_in, uv + i * texel * uniforms.direction, 0).rgba * weights[i];
    color += textureLod(s_in, uv - i * texel * uniforms.direction, 0).rgba * weights[i];
  }

  imageStore(i_out, gid, color);
}