#version 460 core

layout(binding = 0) uniform sampler2D s_illumination;
layout(binding = 1) uniform sampler2D s_gAlbedo;

layout(binding = 0) uniform restrict writeonly image2D i_outIndirect;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  ivec2 targetDim = imageSize(i_outIndirect);
  if (any(greaterThanEqual(gid, targetDim)))
  {
    return;
  }

  vec2 uv = (vec2(gid) + 0.5) / targetDim;

  vec3 albedo = textureLod(s_gAlbedo, uv, 0).rgb;
  vec3 ambient = textureLod(s_illumination, uv, 0).rgb;

  imageStore(i_outIndirect, gid, vec4(ambient * albedo, 1.0));
}