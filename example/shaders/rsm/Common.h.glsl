#ifndef COMMON_H
#define COMMON_H

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

float GetViewDepth(float depth, mat4 proj)
{
  // Returns linear depth in [near, far]
  return proj[3][2] / (proj[2][2] + (depth * 2.0 - 1.0));
}

float DepthWeight(float depthPrev, float depthCur, vec3 normalCur, vec3 viewDir, mat4 proj, float phi)
{
  float linearDepthPrev = GetViewDepth(depthPrev, proj);
  
  float linearDepth = GetViewDepth(depthCur, proj);
  
  float angleFactor = max(0.25, -dot(normalCur, viewDir));

  float diff = abs(linearDepthPrev - linearDepth);
  return exp(-diff * angleFactor / phi);
}

float NormalWeight(vec3 normalPrev, vec3 normalCur, float phi)
{
  //float d = max(0, dot(normalCur, normalPrev));
  //return d * d;
  vec3 dd = normalPrev - normalCur;
  float d = dot(dd, dd);
  return exp(-d / phi);
}

float LuminanceWeight(float luminanceOther, float luminanceCenter, float variance, float phi)
{
  variance = max(variance, 0.0);
  float num = abs(luminanceOther - luminanceCenter);
  float den = phi * sqrt(variance) + 0.01;
  return exp(-num / den);
}

float Luminance(vec3 c)
{
  return dot(c, vec3(0.213,  0.715, 0.072));
}

vec3 Bilerp(vec3 _00, vec3 _01, vec3 _10, vec3 _11, vec2 weight)
{
  vec3 bottom = mix(_00, _10, weight.x);
  vec3 top = mix(_01, _11, weight.x);
  return mix(bottom, top, weight.y);
}

vec2 Bilerp(vec2 _00, vec2 _01, vec2 _10, vec2 _11, vec2 weight)
{
  vec2 bottom = mix(_00, _10, weight.x);
  vec2 top = mix(_01, _11, weight.x);
  return mix(bottom, top, weight.y);
}

float Bilerp(float _00, float _01, float _10, float _11, vec2 weight)
{
  float bottom = mix(_00, _10, weight.x);
  float top = mix(_01, _11, weight.x);
  return mix(bottom, top, weight.y);
}

#endif // COMMON_H