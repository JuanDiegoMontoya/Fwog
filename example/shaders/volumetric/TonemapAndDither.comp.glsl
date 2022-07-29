#version 450 core

layout(binding = 0) uniform sampler2D s_sceneColor;
layout(binding = 1) uniform sampler2D s_noise;

layout(binding = 0) uniform writeonly restrict image2D i_finalColor;

vec3 aces_approx(vec3 v)
{
  v *= 0.6f;
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp((v * (a * v + b)) / (v * (c * v + d) + e), 0.0f, 1.0f);
}

vec3 linear_to_srgb(vec3 linearColor)
{
  bvec3 cutoff = lessThan(linearColor, vec3(0.0031308));
  vec3 higher = vec3(1.055) * pow(linearColor, vec3(1.0 / 2.4)) - vec3(0.055);
  vec3 lower = linearColor * vec3(12.92);

  return mix(higher, lower, cutoff);
}

vec3 apply_dither(vec3 color, vec2 uv)
{
  vec2 uvNoise = uv * (vec2(textureSize(s_sceneColor, 0)) / vec2(textureSize(s_noise, 0)));
  vec3 noiseSample = textureLod(s_noise, uvNoise, 0).rgb;
  return color + vec3((noiseSample - 0.5) / 255.0);
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec2 targetDim = imageSize(i_finalColor);
  if (any(greaterThanEqual(gid, targetDim)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim;

  vec3 hdrColor = textureLod(s_sceneColor, uv, 0).rgb;
  vec3 ldrColor = aces_approx(hdrColor);
  vec3 srgbColor = linear_to_srgb(ldrColor);
  vec3 ditheredColor = apply_dither(srgbColor, uv);

  imageStore(i_finalColor, gid, vec4(ditheredColor, 1.0));
}