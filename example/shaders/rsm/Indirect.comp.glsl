#version 460 core

#define PI     3.14159265
#define TWO_PI (2.0 * PI)

layout(binding = 0) uniform sampler2D s_inIndirect;
layout(binding = 1) uniform sampler2D s_gAlbedo;
layout(binding = 2) uniform sampler2D s_gNormal;
layout(binding = 3) uniform sampler2D s_gDepth;
layout(binding = 4) uniform sampler2D s_rsmFlux;
layout(binding = 5) uniform sampler2D s_rsmNormal;
layout(binding = 6) uniform sampler2D s_rsmDepth;

layout(binding = 0) uniform writeonly image2D i_outIndirect;

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
  // move rsmPos in negative of normal by small constant amount(?)
  // rsmWorldPos -= rsmNormal * .01;
  float geometry = max(0.0, dot(rsmNormal, surfaceWorldPos - rsmWorldPos)) *
                   max(0.0, dot(surfaceNormal, rsmWorldPos - surfaceWorldPos));

  // Clamp distance to prevent singularity.
  float d = max(distance(surfaceWorldPos, rsmWorldPos), 0.01);

  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 surfaceAlbedo, vec3 surfaceNormal, vec3 surfaceWorldPos)
{
  vec3 sumC = {0, 0, 0};

  // Compute the position of this surface point projected into the RSM's UV space
  const vec4 rsmClip = rsm.sunViewProj * vec4(surfaceWorldPos, 1.0);
  const vec2 rsmUV = (rsmClip.xy / rsmClip.w) * .5 + .5;

  // Calculate the area of the world-space disk we are integrating over.
  float rMaxWorld = distance(UnprojectUV(0.0, rsmUV, rsm.invSunViewProj),
                             UnprojectUV(0.0, vec2(rsmUV.x + rsm.rMax, rsmUV.y), rsm.invSunViewProj));

  // Samples need to be normalized based on the radius that is sampled, otherwise changing rMax will affect the brightness.
  float normalizationFactor = 2.0 * rMaxWorld * rMaxWorld;

  for (int i = 0; i < rsm.samples; i++)
  {
    // Get two random numbers.
    vec2 xi = Hammersley(i, rsm.samples);
    float r = xi.x;
    float theta = xi.y * TWO_PI;
    vec2 pixelLightUV = rsmUV + vec2(r * cos(theta), r * sin(theta)) * rsm.rMax;
    float weight = xi.x;

    float rsmDepth = textureLod(s_rsmDepth, pixelLightUV, 0.0).x;
    if (rsmDepth == 1.0) continue;
    vec3 rsmFlux = textureLod(s_rsmFlux, pixelLightUV, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, pixelLightUV, 0.0).xyz;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, pixelLightUV, rsm.invSunViewProj);

    sumC += ComputePixelLight(surfaceWorldPos, surfaceNormal, rsmFlux, rsmWorldPos, rsmNormal) * weight;
  }

  return normalizationFactor * sumC * surfaceAlbedo / rsm.samples;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  gid *= 2;

  if (rsm.currentPass == 1)
    gid++;
  if (rsm.currentPass == 2)
    gid.x++;
  if (rsm.currentPass == 3)
    gid.y++;

  if (any(greaterThanEqual(gid, rsm.targetDim)))
    return;
  vec2 texel = 1.0 / rsm.targetDim;
  vec2 uv = (vec2(gid) + 0.5) / rsm.targetDim;

  vec3 albedo = texelFetch(s_gAlbedo, gid, 0).rgb;
  vec3 normal = texelFetch(s_gNormal, gid, 0).xyz;
  float depth = texelFetch(s_gDepth, gid, 0).x;
  vec3 worldPos = UnprojectUV(depth, uv, invViewProj);

  if (depth == 1.0)
  {
    imageStore(i_outIndirect, gid, vec4(0.0));
    return;
  }

  vec3 ambient = vec3(0);

  if (rsm.currentPass == 0)
  {
    ambient = ComputeIndirectIrradiance(albedo, normal, worldPos);
  }
  else
  {
    // look at corners and identify if any two opposing ones can be interpolated
    ivec2 offsets[4];
    if (rsm.currentPass == 1)
    {
      offsets = ivec2[4](ivec2(-1, 1), ivec2(1, 1), ivec2(-1, -1), ivec2(1, -1));
    }
    else if (rsm.currentPass > 1)
    {
      offsets = ivec2[4](ivec2(0, 1), ivec2(0, -1), ivec2(-1, 0), ivec2(1, 0));
    }

    // compute weights for each
    float accum_weight = 0;
    vec3 accum_color = vec3(0);
    for (int i = 0; i < 4; i++)
    {
      ivec2 corner = gid + offsets[i];
      if (any(greaterThanEqual(corner, rsm.targetDim)) || any(lessThan(corner, ivec2(0))))
        continue;

      vec3 c = texelFetch(s_inIndirect, corner, 0).rgb;

      vec3 n = texelFetch(s_gNormal, corner, 0).xyz;
      vec3 dn = normal - n;
      float n_weight = exp(-dot(dn, dn) / 1.0);

      vec3 p = UnprojectUV(texelFetch(s_gDepth, corner, 0).x, uv + vec2(offsets[i]) * texel, invViewProj);
      vec3 dp = worldPos - p;
      float p_weight = exp(-dot(dp, dp) / 0.4);

      float weight = n_weight * p_weight;
      accum_color += c * weight;
      accum_weight += weight;
    }

    ambient = accum_color / accum_weight;

    // if quality of neighbors is too low, instead compute indirect irradiance
    if (accum_weight <= 3.0)
    {
      ambient = ComputeIndirectIrradiance(albedo, normal, worldPos);
      // ambient = vec3(1, 0, 0); // debug
    }
  }

  imageStore(i_outIndirect, gid, vec4(ambient, 1.0));
}