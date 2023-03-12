#version 460 core

#define PI     3.14159265
#define TWO_PI (2.0 * PI)

layout(binding = 0) uniform sampler2D s_inIndirect;
layout(binding = 2) uniform sampler2D s_gNormal;
layout(binding = 3) uniform sampler2D s_gDepth;
layout(binding = 4) uniform sampler2D s_rsmFlux;
layout(binding = 5) uniform sampler2D s_rsmNormal;
layout(binding = 6) uniform sampler2D s_rsmDepth;
layout(binding = 7) uniform sampler2D s_blueNoise;

layout(binding = 0) uniform restrict writeonly image2D i_outIndirect;

layout(binding = 0, std140) uniform GlobalUniforms
{
  mat4 viewProj;
  mat4 invViewProj;
  mat4 proj;
  vec4 cameraPos;
};

layout(binding = 1, std140) uniform RSMUniforms
{
  mat4 sunViewProj;
  mat4 invSunViewProj;
  ivec2 targetDim;  // input and output texture dimensions
  float rMax;       // max radius for which indirect lighting will be considered
  uint currentPass; // used to determine which pixels to shade
  uint samples;
  vec2 random;
} rsm;

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(float(i) / float(N), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
}

vec3 ComputePixelLight(vec3 surfaceWorldPos, vec3 surfaceNormal, vec3 rsmFlux, vec3 rsmWorldPos, vec3 rsmNormal)
{
  // Move rsmPos in negative of normal by small constant amount(?). This is mentioned in the paper,
  // but does not seem useful.
  // rsmWorldPos -= rsmNormal * .01;
  float geometry = max(0.0, dot(rsmNormal, surfaceWorldPos - rsmWorldPos)) *
                   max(0.0, dot(surfaceNormal, rsmWorldPos - surfaceWorldPos));

  // Clamp distance to prevent singularity.
  float d = max(distance(surfaceWorldPos, rsmWorldPos), 0.03);

  // Inverse square attenuation. d^4 is due to us not normalizing the two ray directions in the numerator.
  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 surfaceNormal, vec3 surfaceWorldPos, vec2 noise)
{
  vec3 sumC = {0, 0, 0};

  const vec4 rsmClip = rsm.sunViewProj * vec4(surfaceWorldPos, 1.0);
  const vec2 rsmUV = (rsmClip.xy / rsmClip.w) * .5 + .5;

  float rMaxWorld = distance(UnprojectUV(0.0, rsmUV, rsm.invSunViewProj),
                             UnprojectUV(0.0, vec2(rsmUV.x + rsm.rMax, rsmUV.y), rsm.invSunViewProj));

  // Samples need to be normalized based on the radius that is sampled, otherwise changing rMax will affect the brightness.
  float normalizationFactor = 2.0 * rMaxWorld * rMaxWorld;

  for (int i = 0; i < rsm.samples; i++)
  {
    vec2 xi = Hammersley(i, rsm.samples);
    // xi can be randomly rotated based on screen position. The original paper does not use screen-space noise to
    // offset samples, but we do because it is important for the new filtering step.

    // Apply Cranley-Pattern rotation/toroidal shift with per-pixel noise
    xi = fract(xi + noise.xy);

    float r = xi.x;
    float theta = xi.y * TWO_PI;
    vec2 pixelLightUV = rsmUV + vec2(r * cos(theta), r * sin(theta)) * rsm.rMax;
    float weight = r;

    float rsmDepth = textureLod(s_rsmDepth, pixelLightUV, 0.0).x;
    if (rsmDepth == 1.0) continue;
    vec3 rsmFlux = textureLod(s_rsmFlux, pixelLightUV, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, pixelLightUV, 0.0).xyz;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, pixelLightUV, rsm.invSunViewProj);

    sumC += ComputePixelLight(surfaceWorldPos, surfaceNormal, rsmFlux, rsmWorldPos, rsmNormal) * weight;
  }

  return normalizationFactor * sumC / rsm.samples;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(gid, rsm.targetDim)))
  {
    return;
  }

  vec2 texel = 1.0 / rsm.targetDim;
  vec2 uv = (vec2(gid) + 0.5) / rsm.targetDim;

  vec2 noise = rsm.random + textureLod(s_blueNoise, (vec2(gid) + 0.5) / textureSize(s_blueNoise, 0), 0).xy;

  vec3 normal = texelFetch(s_gNormal, gid, 0).xyz;
  float depth = texelFetch(s_gDepth, gid, 0).x;
  vec3 worldPos = UnprojectUV(depth, uv, invViewProj);

  if (depth == 1.0)
  {
    imageStore(i_outIndirect, gid, vec4(0.0));
    return;
  }

  vec3 ambient = ComputeIndirectIrradiance(normal, worldPos, noise);

  imageStore(i_outIndirect, gid, vec4(ambient, 1.0));
}