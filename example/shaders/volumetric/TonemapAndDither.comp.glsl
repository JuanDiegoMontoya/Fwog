#version 450 core

layout(binding = 0) uniform sampler2D s_sceneColor;
layout(binding = 1) uniform sampler2D s_noise;

layout(binding = 0) uniform writeonly restrict image2D i_finalColor;

// ACES fitted
// from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl

const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFitted(vec3 color)
{
    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = color * ACESOutputMat;

    // Clamp to [0, 1]
    color = clamp(color, 0.0, 1.0);

    return color;
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
  vec3 ldrColor = ACESFitted(hdrColor);
  vec3 srgbColor = linear_to_srgb(ldrColor);
  vec3 ditheredColor = apply_dither(srgbColor, uv);

  imageStore(i_finalColor, gid, vec4(ditheredColor, 1.0));
}