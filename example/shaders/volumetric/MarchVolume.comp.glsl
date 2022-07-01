#version 460 core
#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

layout(binding = 0) uniform sampler3D s_colorDensityVolume;
layout(binding = 0) uniform writeonly image3D i_inScatteringTransmittanceVolume;

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
    float stepSize = distance(pPrev, pCur);
    pPrev = pCur;

    vec4 froxelInfo = textureLod(s_colorDensityVolume, uvw, 0);
    vec3 froxelLight = froxelInfo.rgb;
    float froxelDensity = froxelInfo.a;

    densityAccum += froxelDensity * stepSize;
    float transmittance = beer(densityAccum);

    // 10*stepSize makes the accumulation independent of volume size and depth distribution
    inScatteringAccum += 10 * stepSize * transmittance * froxelLight;

    imageStore(i_inScatteringTransmittanceVolume, ivec3(gid, i), vec4(inScatteringAccum, transmittance));
  }
}