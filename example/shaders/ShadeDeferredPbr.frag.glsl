#version 460 core

layout(binding = 0) uniform sampler2D s_gAlbedo;
layout(binding = 1) uniform sampler2D s_gNormal;
layout(binding = 2) uniform sampler2D s_gDepth;
layout(binding = 3) uniform sampler2D s_rsmIndirect;
layout(binding = 4) uniform sampler2D s_rsmDepth;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec3 o_color;

layout(binding = 0, std140) uniform GlobalUniforms
{
  mat4 viewProj;
  mat4 invViewProj;
  mat4 proj;
  vec4 cameraPos;
};

layout(binding = 1, std140) uniform ShadingUniforms
{
  mat4 sunViewProj;
  vec4 sunDir;
  vec4 sunStrength;
}shadingUniforms;

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

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

float hash(vec2 n)
{ 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(float(i) / float(N), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
}

float Shadow(vec4 clip, vec3 normal, vec3 lightDir)
{
  vec2 uv = clip.xy * .5 + .5;
  if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
  {
    return 0;
  }

  // Analytically compute slope-scaled bias
  const float maxBias = 0.0008;
  const float quantize = 2.0 / (1 << 23);
  ivec2 res = textureSize(s_rsmDepth, 0);
  float b = 1.0 / max(res.x, res.y) / 2.0;
  float NoD = clamp(-dot(shadingUniforms.sunDir.xyz, normal), 0.0, 1.0);
  float bias = quantize + b * length(cross(-shadingUniforms.sunDir.xyz, normal)) / NoD;
  bias = min(bias, maxBias);

  float lightOcclusion = 0.0;

  const int SAMPLES = 4;
  for (int i = 0; i < SAMPLES; i++)
  {
    float viewDepth = clip.z * .5 + .5;

    vec2 xi = fract(Hammersley(i, SAMPLES) + hash(gl_FragCoord.xy));
    float r = xi.x;
    float theta = xi.y * 2.0 * 3.14159;
    vec2 offset = 0.002 * vec2(r * cos(theta), r * sin(theta));
    float lightDepth = textureLod(s_rsmDepth, uv + offset, 0).x;

    lightDepth += bias;
    
    if (lightDepth >= viewDepth)
    {
      lightOcclusion += 1.0;
    }
  }

  return lightOcclusion / SAMPLES;
}

float GetSquareFalloffAttenuation(vec3 posToLight, float lightInvRadius)
{
  float distanceSquared = dot(posToLight, posToLight);
  float factor = distanceSquared * lightInvRadius * lightInvRadius;
  float smoothFactor = max(1.0 - factor * factor, 0.0);
  return (smoothFactor * smoothFactor) / max(distanceSquared, 1e-4);
}

vec3 LocalLightIntensity(vec3 fragWorldPos, vec3 N, vec3 V, vec3 albedo)
{
  vec3 color = { 0, 0, 0 };

  for (int i = 0; i < lightBuffer.lights.length(); i++)
  {
    Light light = lightBuffer.lights[i];
    vec3 L = normalize(light.position.xyz - fragWorldPos);
    float NoL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * NoL * light.intensity;

    vec3 H = normalize(V + L);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = albedo * spec * light.intensity;

    vec3 localColor = diffuse + specular;
    localColor *= GetSquareFalloffAttenuation(light.position.xyz - fragWorldPos, light.invRadius);

    color += localColor;
  }

  return color;
}

void main()
{
  vec3 albedo = textureLod(s_gAlbedo, v_uv, 0.0).rgb;
  vec3 normal = textureLod(s_gNormal, v_uv, 0.0).xyz;
  float depth = textureLod(s_gDepth, v_uv, 0.0).x;

  if (depth == 1.0)
  {
    o_color = albedo;
    return;
  }

  vec3 fragWorldPos = UnprojectUV(depth, v_uv, invViewProj);
  
  vec3 incidentDir = -shadingUniforms.sunDir.xyz;
  float cosTheta = max(0.0, dot(incidentDir, normal));
  vec3 diffuse = albedo * cosTheta * shadingUniforms.sunStrength.rgb;

  float shadow = Shadow(shadingUniforms.sunViewProj * vec4(fragWorldPos, 1.0), normal, shadingUniforms.sunDir.xyz);
  
  vec3 viewDir = normalize(cameraPos.xyz - fragWorldPos);
  //vec3 reflectDir = reflect(-incidentDir, normal);
  vec3 halfDir = normalize(viewDir + incidentDir);
  float spec = pow(max(dot(normal, halfDir), 0.0), 64.0);
  vec3 specular = albedo * spec * shadingUniforms.sunStrength.rgb;

  //vec3 ambient = vec3(.03) * albedo;
  vec3 ambient = /*vec3(.01) * albedo*/ + textureLod(s_rsmIndirect, v_uv, 0).rgb;
  vec3 finalColor = shadow * (diffuse + specular) + ambient;
  
  finalColor += LocalLightIntensity(fragWorldPos, normal, viewDir, albedo);

  // tone mapping (optional)
  //finalColor = finalColor / (1.0 + finalColor);
  o_color = finalColor;
}