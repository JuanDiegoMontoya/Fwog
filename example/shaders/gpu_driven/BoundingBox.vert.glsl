#version 460 core

layout(binding = 0, std140) uniform GlobalUniforms
{
  mat4 viewProj;
  mat4 invViewProj;
  vec4 cameraPos;
}globalUniforms;

struct ObjectUniforms
{
  mat4 model;
  uint materialIdx;
};

layout(binding = 0, std430) readonly buffer ObjectUniformsBuffer
{
  ObjectUniforms objects[];
};

struct BoundingBox
{
  vec3 offset;
  vec3 halfExtent;
};

layout(binding = 2, std430) readonly buffer BoundingBoxesBuffer
{
  BoundingBox boundingBoxes[];
};

vec3 CreateCube(in uint vertexID)
{
  uint b = 1 << vertexID;
  return vec3((0x287a & b) != 0, (0x02af & b) != 0, (0x31e3 & b) != 0);
}

void main()
{
  uint i = gl_BaseInstance + gl_InstanceID;
  vec3 a_pos = CreateCube(gl_VertexID) - .5; // gl_VertexIndex for Vulkan
  ObjectUniforms obj = objects[i];
  a_pos *= boundingBoxes[i].halfExtent * 2.0;
  a_pos += boundingBoxes[i].offset;
  vec3 position = (obj.model * vec4(a_pos, 1.0)).xyz;
  gl_Position = globalUniforms.viewProj * vec4(position, 1.0);
}