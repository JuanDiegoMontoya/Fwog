#version 460 core

#define PI 3.141592653589793238462643
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
}rsm;

float LinearizeDepth(float depth)
{
	return proj[3][2] / (proj[2][2] + (2.0 * depth - 1.0));
	/*float n = 0.1;
	float f = 100.0;
	float z_ndc = 2.0 * depth - 1.0;
	return 2.0 * n * f / (f + n - z_ndc * (f - n));*/
}

vec2 ProjectUV(vec3 worldPos, mat4 xProj)
{
  vec4 clipPos = xProj * vec4(worldPos, 1.0);
  return (clipPos.xy / clipPos.w) * .5 + .5;
}

vec3 UnprojectUV(float depth, vec2 uv, mat4 invXProj)
{
  float z = depth * 2.0 - 1.0; // OpenGL Z convention
  vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 world = invXProj * ndc;
  return world.xyz / world.w;
}

float Random(float co)
{
  return fract(sin(co*(91.3458)) * 47453.5453);
}

vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}

//takes [0; 1]^2 spherical, maps to cartesian unit sphere
vec3 NormalizedSphericalToCartesian(in vec2 uv)
{
    float cos_theta = 2.0*uv.x-1.0;
    float phi = 2.0*PI*uv.y;
    float sin_theta = sqrt(1.0-cos_theta*cos_theta);
    float sin_phi = sin(phi);
    float cos_phi = cos(phi);
    
    return vec3(sin_theta*cos_phi, cos_theta, sin_theta*sin_phi);
}

//takes [0; 1]^2 vector, maps to cosine weighted hemisphere oriented along unit vector n
vec3 CosineWeightedHemisphereMapping(in vec2 uv, in vec3 n)
{
    vec3 p = NormalizedSphericalToCartesian(uv);
    return n+p;
}

//takes [0; 1]^2 vector, maps to hemisphere oriented along unit vector n
vec3 HemisphereMapping(in vec2 uv, in vec3 n)
{
    vec3 p = NormalizedSphericalToCartesian(uv);
    return p*sign(dot(p,n));
}

vec3 ComputePixelLight(vec3 surfaceWorldPos, vec3 surfaceNormal, vec3 rsmFlux, vec3 rsmWorldPos, vec3 rsmNormal)
{
  // move rsmPos in negative of normal by small constant amount(?)
  //rsmWorldPos += -rsmNormal * .01;
  vec3 sampleDir = rsmWorldPos - surfaceWorldPos;
  float cosSrc = max(0.0, dot(surfaceNormal, sampleDir));
  float cosDest = max(0.0, dot(rsmNormal, -sampleDir));
  float geometry = cosSrc * cosSrc * cosDest;
  //taking max to avoid fireflies and nans from division at the cost of more bias
  float d = max(distance(surfaceWorldPos, rsmWorldPos), 0.05);
  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 surfaceAlbedo, vec3 surfaceNormal, vec3 surfaceWorldPos, vec3 noise)
{
  vec3 sumC = { 0, 0, 0 };
  for (int i = 0; i < rsm.samples; i++)
  {
    // xi can be randomly rotated based on screen position
    vec2 xi = Hammersley(i, rsm.samples);
	// and we are absolutely going to rotate it with blue noise
	xi = mod(xi+vec2(noise.xy), vec2(1.0));
	float rand = mod(Random(float(i))+noise.z, 1.0);
	vec3 sample_pos = surfaceWorldPos+normalize(CosineWeightedHemisphereMapping(xi, surfaceNormal))*rsm.rMax*5.0*rand;
    vec2 sample_uv = ProjectUV(sample_pos, rsm.sunViewProj);

    vec3 rsmFlux = textureLod(s_rsmFlux, sample_uv, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, sample_uv, 0.0).xyz;
    float rsmDepth = textureLod(s_rsmDepth, sample_uv, 0.0).x;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, sample_uv, rsm.invSunViewProj);
	
	//visibility query (bad)
	/*bool visible = true;
	float depth_begin = textureLod(s_rsmDepth, ProjectUV(surfaceWorldPos, rsm.sunViewProj), 0.0).x;
	float depth_end = textureLod(s_rsmDepth, sample_uv, 0.0).x;
	float thick = 0.1;
	for(int j = 0; j < 5; ++j)
	{
		const float first_step = 1.0/3.0;
		float step_factor = j/3.0 + first_step;
		float min_depth = (depth_begin, depth_end, step_factor)-0.1;
		vec3 sample_vis_pos = mix(surfaceWorldPos, sample_pos, vec3(step_factor));
		float depth_vis = textureLod(s_rsmDepth, ProjectUV(sample_vis_pos, rsm.sunViewProj), 0.0).x;
		if(depth_vis < min_depth && min_depth - depth_vis < thick)
		{
			visible = false;
			break;
		}
	}
	if(visible)*/
		sumC += ComputePixelLight(surfaceWorldPos, surfaceNormal, rsmFlux, rsmWorldPos, rsmNormal);	
  }
  return sumC/* * surfaceAlbedo*/ / rsm.samples;
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
	else if (rsm.currentPass == 5)
	{
		ambient = texelFetch(s_inIndirect, gid, 0).xyz;
		ambient *= albedo;
	}
  }
  
  imageStore(i_outIndirect, gid, vec4(ambient, 1.0));
}