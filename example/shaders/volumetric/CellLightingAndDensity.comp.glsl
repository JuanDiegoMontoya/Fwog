#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

#include "Frog.h"

layout(binding = 0) uniform sampler2D s_exponentialShadowDepth;
layout(binding = 1) uniform sampler1D s_fogScattering;
layout(binding = 0) uniform writeonly image3D i_target;

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

float snoise(vec4 v);

float sdBox(vec3 p, vec3 b)
{
  vec3 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

vec3 phaseTex(float cosTheta)
{
  float theta = acos(clamp(cosTheta, -1, 1));
  float u = theta / 3.1415926;
  vec3 intensity = textureLod(s_fogScattering, u, 0).rgb;
  return intensity;
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

vec4 FogAtPoint(vec3 wPos)
{
  // ground fog
  vec3 t = vec3(.2, 0.1, .3) * uniforms.time;
  float d = max((snoise(vec4(wPos * 0.11 + t, t * 1.2)) + 0.5), 0.0);
  d *= uniforms.groundFogDensity;

  // Fade out fog if too low or too high.
  d *= (1.0 - smoothstep(0, 10, wPos.y)) * (smoothstep(-15, 0, wPos.y));

  // Fade out fog if too far from center of the world.
  d *= 1.0 - smoothstep(0, 10, distance(abs(wPos.xz), vec2(0)) - 25);

  // Clouds. Only work if volume far plane is quite large.
  // float cd = max((snoise(vec4(wPos * 0.001 + t * .1, t * 0.05)) + 0.1) * .05, 0.0);
  // cd += max((snoise(vec4(wPos * 0.01 + t * .2, t * 0.05)) + 0.1) * .01, 0.0);
  // cd *= (1.0 - smoothstep(10, 30, abs(wPos.y - 100.)));
  // d += cd;
  
  vec3 c = vec3(1, 1, 1); // base color

  // Fog cube.
  d += 1.0 - smoothstep(0.0, .25, sdBox(wPos - vec3(3., 2., 0.), vec3(0.75)));

  if (uniforms.frog != 0)
  {
    vec3 frogPos = vec3(1, 5, 2);
    frogPos.x += sin(uniforms.time) * 2;
    frogPos.y += cos(uniforms.time) * 2;
    frog_sdfRet ret = frog_map(0.125 * (wPos - frogPos));
    float froge = 1.0 - smoothstep(0.0, 0.05, ret.sdf);
    {
      c = mix(c, frog_idtocol(ret.id), froge);
      d += froge * 1.0;
    }
  }

  return vec4(c, d);
}

vec4 DensityToLight(vec3 start, vec3 end, int steps)
{
  const vec3 dir = normalize(end - start);
  const float stepSize = distance(end, start) / steps;

  vec3 inScatteringAccum = vec3(0);
  float densityAccum = 0;

  vec3 curPos = start;
  for (int i = 0; i < steps; i++)
  {
    curPos += dir * stepSize;

    vec4 cd = FogAtPoint(curPos);
    densityAccum += cd.w * stepSize;
  }

  return vec4(inScatteringAccum, densityAccum);
}

vec3 LocalLightIntensity(vec3 wPos, vec3 V, float froxelDensity, float k)
{
  vec3 lightAccum = { 0, 0, 0 };

  // Accumulate contribuation from each local light.
  for (int i = 0; i < lightBuffer.lights.length(); i++)
  {
    Light light = lightBuffer.lights[i];

    vec3 posToLight = light.position.xyz - wPos;
    float distanceSquared = dot(posToLight, posToLight);
    vec3 localLight = GetSquareFalloffAttenuation(distanceSquared, light.invRadius).xxx;

    // Assume media density is constant from this cell to the light source.
    // Probably close enough in most situations.
    float d = sqrt(distanceSquared) * froxelDensity;
    //float d = ScatteringAndDensityToLight(light.intensity.rgb, wPos, light.position.xyz, 1).w;
    float localInScatteringCoefficient = beer(d);

    vec3 L = normalize(light.position.xyz - wPos);
    localLight *= phaseSchlick(k, dot(V, L));
    //localLight *= phaseTex(dot(V, L));

    localLight *= localInScatteringCoefficient;
    
    lightAccum += localLight * light.intensity.rgb;
  }

  return lightAccum;
}

vec3 CalculateFroxelLighting(vec3 froxelColor, float froxelDensity, vec3 wPos)
{
    float shadow = ShadowESM(uniforms.sunViewProj * vec4(wPos, 1.0));

    vec3 viewDir = normalize(wPos - uniforms.viewPos);
    float k = gToK(uniforms.anisotropyG);

    float VoL = dot(-viewDir, uniforms.sunDir);

    vec3 sunlight = shadow * uniforms.sunColor;
    if (uniforms.useScatteringTexture != 0)
      sunlight *= phaseTex(VoL);
    else
      sunlight *= phaseSchlick(k, VoL);

    sunlight *= beer(DensityToLight(wPos - uniforms.sunDir * 50, wPos, 5).w);

    // Local light(s) contribution.
    vec3 localLight = LocalLightIntensity(wPos, viewDir, froxelDensity, k);

    // Ambient direct scattering hack because this is not PBR.
    // Yes, the ambient term has up to 50% shadowing. This is because it looks cool.
    float ambientFactor = max(0.002, 0.01 * smoothstep(0., 1., min(1.0, .5 + dot(uniforms.sunDir, vec3(0, -1, 0)))));
    //ambientFactor = 0;
    float ambient = ambientFactor;// * max(shadow, 0.5);

    return froxelColor * froxelDensity * (sunlight + localLight + ambient);
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
void main()
{
  ivec3 gid = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 targetDim = imageSize(i_target);
  if (any(greaterThanEqual(gid, targetDim)))
    return;
  vec3 uvw = (vec3(gid) + 0.5) / targetDim;

  // Apply our own curve by squaring the linear depth, then convert to inverted window-space Z and unproject it to get world position.
  float zInv = InvertDepthZO(uvw.z * uvw.z, uniforms.volumeNearPlane, uniforms.volumeFarPlane);
  vec3 wPos = UnprojectUVZO(zInv, uvw.xy, uniforms.invViewProjVolume);

  vec4 colorAndDensity = FogAtPoint(wPos);
  vec3 fogColor = colorAndDensity.rgb;
  float fogDensity = colorAndDensity.w;

  // sphere
  //d += 1.0 - smoothstep(3, 5, distance(p, vec3(0, 5, 0)));
  vec3 light = CalculateFroxelLighting(fogColor, fogDensity, wPos);

  imageStore(i_target, gid, vec4(light, fogDensity));
}




//	Simplex 4D Noise 
//	by Ian McEwan, Ashima Arts
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

vec4 grad4(float j, vec4 ip)
{
  const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
  vec4 p,s;

  p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  s = vec4(lessThan(p, vec4(0.0)));
  p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

  return p;
}

float snoise(vec4 v)
{
  const vec2  C = vec2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
  // First corner
  vec4 i  = floor(v + dot(v, C.yyyy) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

  // Other corners

  // Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;

  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
  //  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

  //  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C 
  vec4 x1 = x0 - i1 + 1.0 * C.xxxx;
  vec4 x2 = x0 - i2 + 2.0 * C.xxxx;
  vec4 x3 = x0 - i3 + 3.0 * C.xxxx;
  vec4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

  // Permutations
  i = mod(i, 289.0); 
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));
  // Gradients
  // ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
  // 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

  // Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

  // Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}