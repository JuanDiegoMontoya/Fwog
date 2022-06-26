#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "common.h"

layout(binding = 0) uniform sampler3D s_colorDensityVolume;
layout(binding = 1) uniform sampler2D s_exponentialShadowDepth;
layout(binding = 2) uniform sampler1D s_fogScattering;
layout(binding = 0) uniform writeonly image3D i_inScatteringTransmittanceVolume;

layout(binding = 1, std140) uniform ESM_UNIFORMS
{
  float depthExponent;
}esmUniforms;

struct Light
{
  vec4 position;
  vec3 intensity;
  float invRadius;
};

layout(binding = 0, std430) readonly buffer LightBuffer
{
  Light lights[];
}lightBuffer;

#define PI 3.1415926

// henyey-greenstein phase function for isotropic in-scattering
float phaseHG(float g, float cosTheta)
{
  return (1.0 - g * g) / (4.0 * PI * pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5));
}

float phaseSchlick(float k, float cosTheta)
{
  float denom = 1.0 - k * cosTheta;
  return (1.0 - k * k) / (4.0 * PI * denom * denom);
}

vec3 phaseTex(float cosTheta)
{
  // [1, -1] -> [0, 1]
  float u = 1.0 - (cosTheta * .5 + .5);
  
  vec3 intensity = textureLod(s_fogScattering, u, 0).rgb;

  // limit intensity (hack)
  //return log(1.0 + intensity);
  return intensity / (1.0 + intensity);
}

float gToK(float g)
{
  return clamp(1.55 * g - 0.55 * g * g * g, -0.999, 0.999);
}

float beer(float d)
{
  return exp(-d);
}

float powder(float d)
{
  return 1.0 - exp(-d * 2.0);
}

float ShadowESM(vec4 clip)
{
  vec4 unorm = clip;
  if (any(greaterThan(unorm.xyz, vec3(1.0)))) return 1.0;
  unorm.xy = unorm.xy * .5 + .5;
  float lightDepth = textureLod(s_exponentialShadowDepth, unorm.xy, 0.0).x;
  float eyeDepth = unorm.z;
  return clamp(lightDepth * exp(-esmUniforms.depthExponent * eyeDepth), 0.0, 1.0);
}

float GetSquareFalloffAttenuation(float distanceSquared, float lightInvRadius)
{
  float factor = distanceSquared * lightInvRadius * lightInvRadius;
  float smoothFactor = max(1.0 - factor * factor, 0.0);
  return (smoothFactor * smoothFactor) / max(distanceSquared, 1e-4);
}

vec3 LocalLightIntensity(vec3 wPos, vec3 V, vec3 froxelColor, float froxelDensity, float k)
{
  vec3 color = { 0, 0, 0 };

  // Accumulate contribuation from each local light.
  for (int i = 0; i < lightBuffer.lights.length(); i++)
  {
    Light light = lightBuffer.lights[i];
    
    vec3 diffuse = froxelColor * light.intensity;

    vec3 localColor = diffuse;
    vec3 posToLight = light.position.xyz - wPos;
    float distanceSquared = dot(posToLight, posToLight);
    localColor *= GetSquareFalloffAttenuation(distanceSquared, light.invRadius);

    // Assume media density is constant from this cell to the light source.
    // Probably close enough in most situations.
    float d = sqrt(distanceSquared) * froxelDensity;
    float localInScatteringCoefficient = beer(d);

    vec3 L = normalize(light.position.xyz - wPos);
    localColor *= phaseSchlick(k, dot(V, L));
    //localColor *= phaseTex(dot(V, L));

    localColor *= localInScatteringCoefficient;
    
    color += localColor;
  }

  return color * froxelDensity;
}

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
  ivec3 targetDim = imageSize(i_inScatteringTransmittanceVolume);
  if (any(greaterThanEqual(gid, targetDim.xy)))
    return;
  vec2 uv = (vec2(gid) + 0.5) / targetDim.xy;

  vec3 texel = 1.0 / targetDim;

  vec3 inScatteringAccum = vec3(0.0);
  float densityAccum = 0.0;
  vec3 pPrev = uniforms.viewPos;
  for (int i = 0; i < targetDim.z; i++)
  {
    // uvw is the current voxel in unorm (UV) space. One half is added to i to get the center of the voxel as usual.
    vec3 uvw = vec3(uv, (i + 0.5) * texel.z);
    
    // Starting with linear depth, square it to bias precision towards the viewer.
    // Then, invert the depth as though it were multiplied by the volume projection.
    float zInv = InvertDepthZO(uvw.z * uvw.z, uniforms.volumeNearPlane, uniforms.volumeFarPlane);

    // Unproject the inverted depth to get the world position of this froxel.
    vec3 pCur = UnprojectUVZO(zInv, uv, uniforms.invViewProjVolume);
    
    // The step size is not constant, so we calculate it here.
    float d = distance(pPrev, pCur);
    pPrev = pCur;

    vec4 froxelColorDensity = textureLod(s_colorDensityVolume, uvw, 0);
    vec3 froxelColor = froxelColorDensity.rgb;
    float froxelDensity = froxelColorDensity.a;

    densityAccum += froxelDensity * d;// * uniforms.volumeFarPlane;
    float b = beer(densityAccum);
    float p = powder(densityAccum);
    float inScatteringCoefficient = d * b * p;

    float shadow = ShadowESM(uniforms.sunViewProj * vec4(pCur, 1.0));

    vec3 viewDir = normalize(pCur - uniforms.viewPos);
    float k = gToK(uniforms.isotropyG);

    float VoL = dot(-viewDir, uniforms.sunDir);

    vec3 inScattering = froxelColor * froxelDensity * inScatteringCoefficient * shadow;
    if (uniforms.useScatteringTexture != 0)
      inScattering *= phaseTex(VoL);
    else
      inScattering *= phaseSchlick(k, VoL);

    inScatteringAccum += inScattering;

    // Local light(s) contribution.
    inScatteringAccum += inScatteringCoefficient * LocalLightIntensity(pCur, viewDir, froxelColor, froxelDensity, k);

    // Ambient direct scattering hack because this is not PBR.
    // Yes, the ambient term has up to 50% shadowing. This is because it looks cool.
    float ambient = max(0.003, 0.04 * smoothstep(0., 1., min(1.0, .5 + dot(uniforms.sunDir, vec3(0, -1, 0)))));
    inScatteringAccum += froxelColor * froxelDensity * inScatteringCoefficient * ambient * max(shadow, 0.5);

    // Powder effect (p term) seems to not apply well to transmittance
    float transmittance = b; 
    imageStore(i_inScatteringTransmittanceVolume, ivec3(gid, i), vec4(inScatteringAccum, transmittance));
  }
}