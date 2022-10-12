#version 460 core

#define TWO_PI (2.0 * 3.14159265)

layout(binding = 0) uniform sampler2D s_inIndirect;
layout(binding = 1) uniform sampler2D s_gAlbedo;
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
};

layout(binding = 1, std140) uniform RSMUniforms
{
  mat4 sunViewProj;
  mat4 invSunViewProj;
  ivec2 targetDim;  // input and output texture dimensions
  float rMax;       // max radius for which indirect lighting will be considered
  uint currentPass; // used to determine which pixels to shade
  uint samples;
}rsm;

float LinearizeDepth(float depth)
{
	return proj[3][2] / (proj[2][2] + (2.0 * depth - 1.0));
	/*float n = 0.1;
	float f = 100.0;
	float z_ndc = 2.0 * depth - 1.0;
	return 2.0 * n * f / (f + n - z_ndc * (f - n));*/
}

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}

vec3 ComputePixelLight(vec3 surfaceWorldPos, vec3 surfaceNormal, vec3 rsmFlux, vec3 rsmWorldPos, vec3 rsmNormal)
{
  // move rsmPos in negative of normal by small constant amount(?)
  //rsmWorldPos -= rsmNormal * .01;
  float geometry = max(0.0, dot(rsmNormal, surfaceWorldPos - rsmWorldPos)) * 
                   max(0.0, dot(surfaceNormal, rsmWorldPos - surfaceWorldPos));
  float d = distance(surfaceWorldPos, rsmWorldPos);
  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 surfaceAlbedo, vec3 surfaceNormal, vec3 surfaceWorldPos, vec3 noise)
{
  vec3 sumC = { 0, 0, 0 };

  const vec4 rsmClip = rsm.sunViewProj * vec4(surfaceWorldPos, 1.0);
  const vec2 rsmUV = (rsmClip.xy / rsmClip.w) * .5 + .5;

  for (int i = 0; i < rsm.samples; i++)
  {
    // xi can be randomly rotated based on screen position
    vec2 xi = Hammersley(i, rsm.samples);
	// and we are absolutely going to rotate it with blue noise
	xi = mod(xi+vec2(noise.xy), vec2(1.0));
    float r = xi.x * rsm.rMax;
    float theta = xi.y * TWO_PI;
    vec2 pixelLightUV = rsmUV + vec2(r * cos(theta), r * sin(theta));
    float weight = xi.x * xi.x;

    vec3 rsmFlux = textureLod(s_rsmFlux, pixelLightUV, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, pixelLightUV, 0.0).xyz;
    float rsmDepth = textureLod(s_rsmDepth, pixelLightUV, 0.0).x;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, pixelLightUV, rsm.invSunViewProj);

    sumC += ComputePixelLight(surfaceWorldPos, surfaceNormal, rsmFlux, rsmWorldPos, rsmNormal) * weight;
  }
  
  return sumC * surfaceAlbedo / rsm.samples;
}

vec3 Tap(in sampler2D tex, ivec2 coord, ivec2 offset, vec3 src_normal, float src_depth, inout float sum_weight)
{
	vec3 result = vec3(0);
	vec3 normal = texelFetchOffset(s_gNormal, coord, 0, offset).xyz;
	float depth = LinearizeDepth(texelFetchOffset(s_gDepth, coord, 0, offset).x);
	if(dot(normal, src_normal) >= 0.9 && abs(depth - src_depth) < 0.1)
	{
		result = texelFetchOffset(tex, coord, 0, offset).xyz;
		sum_weight += 1.0;
	}	
	return result;
}

vec3 FilterSubsampled(in sampler2D tex, ivec2 coord, int pattern, int step_size)
{
	float sum_weight = 1.0;
	vec3 sum = vec3(0);
	sum += texelFetch(tex, coord, 0).xyz;
	vec3 normal = texelFetch(s_gNormal, coord, 0).xyz;
	float depth = LinearizeDepth(texelFetch(s_gDepth, coord, 0).x);
	if(pattern == 0) {
		sum += Tap(tex, coord, ivec2(-2,  0) * step_size, normal, depth, sum_weight).xyz;
		sum += Tap(tex, coord, ivec2( 2,  0) * step_size, normal, depth, sum_weight).xyz;
	}
	else {
		sum += Tap(tex, coord, ivec2( 0, -2) * step_size, normal, depth, sum_weight).xyz;
		sum += Tap(tex, coord, ivec2( 0,  2) * step_size, normal, depth, sum_weight).xyz;
	}
	sum += Tap(tex, coord, ivec2(-1,  1) * step_size, normal, depth, sum_weight).xyz;
	sum += Tap(tex, coord, ivec2( 1,  1) * step_size, normal, depth, sum_weight).xyz;
	sum += Tap(tex, coord, ivec2(-1, -1) * step_size, normal, depth, sum_weight).xyz;
	sum += Tap(tex, coord, ivec2( 1, -1) * step_size, normal, depth, sum_weight).xyz;	
	return sum / sum_weight;
}

vec3 FilterBoxX(in sampler2D tex, ivec2 coord)
{
	float sum_weight = 1.0;
    vec3 sum = texelFetch(tex, coord, 0).xyz;
	vec3 normal = texelFetch(s_gNormal, coord, 0).xyz;
	float depth = LinearizeDepth(texelFetch(s_gDepth, coord, 0).x);	
	sum += Tap(tex, coord, ivec2(-2, 0), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2(-1, 0), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2( 1, 0), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2( 2, 0), normal, depth, sum_weight).xyz;
	return sum / sum_weight;
}

vec3 FilterBoxY(in sampler2D tex, ivec2 coord)
{
	float sum_weight = 1.0;
    vec3 sum = texelFetch(tex, coord, 0).xyz;
	vec3 normal = texelFetch(s_gNormal, coord, 0).xyz;
	float depth = LinearizeDepth(texelFetch(s_gDepth, coord, 0).x);	
	sum += Tap(tex, coord, ivec2(0, -2), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2(0, -1), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2(0,  1), normal, depth, sum_weight).xyz
         + Tap(tex, coord, ivec2(0,  2), normal, depth, sum_weight).xyz;
	return sum / sum_weight;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

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
  
  vec3 noise = texture(s_blueNoise, (vec2(gid) + 0.5)/textureSize(s_blueNoise, 0)).xyz;

  vec3 ambient = vec3(0);

  if (rsm.currentPass == 0)
  {
    ambient = ComputeIndirectIrradiance(albedo, normal, worldPos, noise);
  }
  else
  {
	if (rsm.currentPass == 1)
	{
		ambient = FilterSubsampled(s_inIndirect, gid, 0, 5);
	}
	else if (rsm.currentPass == 2)
	{
		ambient = FilterSubsampled(s_inIndirect, gid, 1, 5);
	}
	else if (rsm.currentPass == 3)
	{
		ambient = FilterBoxX(s_inIndirect, gid);
	}
	else if (rsm.currentPass == 4)
	{
		ambient = FilterBoxY(s_inIndirect, gid);
	}
  }
  
  imageStore(i_outIndirect, gid, vec4(ambient, 1.0));
}