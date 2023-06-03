#version 450 core

layout(binding = 0) uniform sampler2D s_sceneColor;
layout(binding = 1) uniform sampler2D s_noise;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_finalColor;

// AgX implementation from here: https://www.shadertoy.com/view/Dt3XDr
vec3 xyYToXYZ(vec3 xyY)
{
    float Y = xyY.z;
    float X = (xyY.x * Y) / xyY.y;
    float Z = ((1.0f - xyY.x - xyY.y) * Y) / xyY.y;

    return vec3(X, Y, Z);
}

vec3 Unproject(vec2 xy)
{
    return xyYToXYZ(vec3(xy.x, xy.y, 1));				
}

mat3 PrimariesToMatrix(vec2 xy_red, vec2 xy_green, vec2 xy_blue, vec2 xy_white)
{
    vec3 XYZ_red = Unproject(xy_red);
    vec3 XYZ_green = Unproject(xy_green);
    vec3 XYZ_blue = Unproject(xy_blue);
    vec3 XYZ_white = Unproject(xy_white);

    mat3 temp = mat3(XYZ_red.x,	  1.0f, XYZ_red.z,
                     XYZ_green.x, 1.0f, XYZ_green.z,
                     XYZ_blue.x,  1.0f, XYZ_blue.z);
    vec3 scale = inverse(temp) * XYZ_white;

    return mat3(XYZ_red * scale.x, XYZ_green * scale.y, XYZ_blue * scale.z);
}

mat3 ComputeCompressionMatrix(vec2 xyR, vec2 xyG, vec2 xyB, vec2 xyW, float compression)
{
    float scale_factor = 1.0f / (1.0f - compression);
    vec2 R = mix(xyW, xyR, scale_factor);
    vec2 G = mix(xyW, xyG, scale_factor);
    vec2 B = mix(xyW, xyB, scale_factor);
    vec2 W = xyW;

    return PrimariesToMatrix(R, G, B, W);
}

float DualSection(float x, float linear, float peak)
{
	// Length of linear section
	float S = (peak * linear);
	if (x < S) {
		return x;
	} else {
		float C = peak / (peak - S);
		return peak - (peak - S) * exp((-C * (x - S)) / peak);
	}
}

vec3 DualSection(vec3 x, float linear, float peak)
{
	x.x = DualSection(x.x, linear, peak);
	x.y = DualSection(x.y, linear, peak);
	x.z = DualSection(x.z, linear, peak);
	return x;
}

vec3 AgX_DS(vec3 color_srgb, float exposure, float saturation, float linear, float peak, float compression)
{
  vec3 workingColor = max(color_srgb, 0.0f) * pow(2.0, exposure);

  mat3 sRGB_to_XYZ = PrimariesToMatrix(vec2(0.64,0.33),
                                       vec2(0.3,0.6), 
                                       vec2(0.15,0.06), 
                                       vec2(0.3127, 0.3290));
  mat3 adjusted_to_XYZ = ComputeCompressionMatrix(vec2(0.64,0.33),
                                                  vec2(0.3,0.6), 
                                                  vec2(0.15,0.06), 
                                                  vec2(0.3127, 0.3290), compression);
  mat3 XYZ_to_adjusted = inverse(adjusted_to_XYZ);
  mat3 sRGB_to_adjusted = sRGB_to_XYZ * XYZ_to_adjusted;

  workingColor = sRGB_to_adjusted * workingColor;
  workingColor = clamp(DualSection(workingColor, linear, peak), 0.0, 1.0);
  
  vec3 luminanceWeight = vec3(0.2126729,  0.7151522,  0.0721750);
  vec3 desaturation = vec3(dot(workingColor, luminanceWeight));
  workingColor = mix(desaturation, workingColor, saturation);
  workingColor = clamp(workingColor, 0.f, 1.f);

  workingColor = inverse(sRGB_to_adjusted) * workingColor;

  return workingColor;
}

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

// sRGB OETF
vec3 linear_to_nonlinear_srgb(vec3 linearColor)
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

void main()
{
  vec3 hdrColor = textureLod(s_sceneColor, v_uv, 0).rgb;
  //vec3 ldrColor = ACESFitted(hdrColor);
  vec3 ldrColor = AgX_DS(hdrColor, -3, 1.0, 0.18, 1, 0.15);
  vec3 srgbColor = linear_to_nonlinear_srgb(ldrColor);
  vec3 ditheredColor = apply_dither(srgbColor, v_uv);

  o_finalColor = vec4(ditheredColor, 1.0);
}