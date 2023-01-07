#version 450 core

layout(binding = 0) uniform sampler2D s_in;

layout(binding = 0) uniform writeonly image2D i_out;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  vec2 uv = (ivec2(gid) + 0.5) / imageSize(i_out);
  imageStore(i_out, gid, textureLod(s_in, uv, 0));
}