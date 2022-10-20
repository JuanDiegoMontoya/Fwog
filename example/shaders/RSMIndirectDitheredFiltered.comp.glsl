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

vec2 QuadraticCircleMapping(vec2 uv)
{
	float r = uv.x;
    float theta = uv.y * TWO_PI;
	return vec2(r * cos(theta), r * sin(theta));
}

float QuadraticCircleMappingWeight(float r)
{
	return r;
}

vec3 ComputePixelLight(vec3 surfaceWorldPos, vec3 surfaceNormal, vec3 rsmFlux, vec3 rsmWorldPos, vec3 rsmNormal)
{
  vec3 rayDir = rsmWorldPos - surfaceWorldPos;
  float geometry = max(0.0, dot(rsmNormal, -rayDir)) * 
                   max(0.0, dot(surfaceNormal, rayDir));
  // clamp to avoid NaNs and fireflies, at the cost of adding negligible anount of bias
  float d = max(length(rayDir), 0.05);
  // raise to 4th power instead of 2nd because ray direction is not normalized and dot products contain its magnitude
  return rsmFlux * geometry / (d * d * d * d);
}

vec3 ComputeIndirectIrradiance(vec3 surfaceAlbedo, vec3 surfaceNormal, vec3 surfaceWorldPos, vec3 noise)
{
  vec3 sumC = { 0, 0, 0 };

  const vec4 rsmClip = rsm.sunViewProj * vec4(surfaceWorldPos, 1.0);
  const vec2 rsmUV = (rsmClip.xy / rsmClip.w) * .5 + .5;
  
  //can be done once on the host side and passed in as a uniform
  float rMaxWorld = distance(UnprojectUV(0.0, rsmUV, rsm.invSunViewProj), UnprojectUV(0.0, vec2(rsmUV.x + rsm.rMax, rsmUV.y), rsm.invSunViewProj));
  float worldSpaceSampledArea = PI*rMaxWorld*rMaxWorld;

  for (int i = 0; i < rsm.samples; i++)
  {
    // xi can be randomly rotated based on screen position
    vec2 xi = Hammersley(i, rsm.samples);
	// and we are absolutely going to rotate it with blue noise
	xi = mod(xi+noise.xy, vec2(1.0));
    vec2 pixelLightUV = rsmUV + QuadraticCircleMapping(xi)*rsm.rMax;
	float weight = QuadraticCircleMappingWeight(xi.x);

    vec3 rsmFlux = textureLod(s_rsmFlux, pixelLightUV, 0.0).rgb;
    vec3 rsmNormal = textureLod(s_rsmNormal, pixelLightUV, 0.0).xyz;
    float rsmDepth = textureLod(s_rsmDepth, pixelLightUV, 0.0).x;
    vec3 rsmWorldPos = UnprojectUV(rsmDepth, pixelLightUV, rsm.invSunViewProj);
	
	//approximate visibility test (super bad)
	/*bool visible = true;
	vec2 uv0 = ProjectUV(surfaceWorldPos, viewProj);
	vec2 uv1 = ProjectUV(rsmWorldPos, viewProj);
	const float thic = 0.8;
	if(all(lessThan(uv1, vec2(1.0))) && all(lessThan(vec2(0.0), uv1)))
	{
		float d0 = LinearizeDepth(textureLod(s_gDepth, uv0, 0).x);
		float d1 = LinearizeDepth(textureLod(s_gDepth, uv1, 0).x);
		for(int j = 0; j < 7; ++j)
		{
			//j*visMaxSteps/(visMaxSteps+1)
			float t = float(j)*0.1;
			float md = mix(d0, d1, t);
			float dt = LinearizeDepth(textureLod(s_gDepth, mix(uv0, uv1, vec2(t)), 0.0).x);
			if(dt < md && dt > md - thic)
			{
				visible = false;
				break;
			}
		}		
	}
	if(visible)*/
      sumC += ComputePixelLight(surfaceWorldPos, surfaceNormal, rsmFlux, rsmWorldPos, rsmNormal) * weight;
  }
  
  return worldSpaceSampledArea * sumC / rsm.samples;
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