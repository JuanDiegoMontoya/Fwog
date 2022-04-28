#pragma once
#include <vector>
#include <optional>
#include <string_view>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <gsdf/Buffer.h>
#include <gsdf/Texture.h>
#include <gsdf/detail/Flags.h>

namespace Utility
{
  struct Vertex
  {
    glm::vec3 position;
    uint32_t normal;
    glm::vec2 texcoord;
  };

  struct CombinedTextureSampler
  {
    std::optional<GFX::Texture> texture;
    std::optional<GFX::TextureView> textureView;
    std::optional<GFX::TextureSampler> sampler;
  };

  enum class MaterialFlagBit
  {
    HAS_BASE_COLOR_TEXTURE = 1 << 0,
  };
  GSDF_DECLARE_FLAG_TYPE(MaterialFlags, MaterialFlagBit, uint32_t)

  struct GpuMaterial
  {
    MaterialFlags flags{};
    float alphaCutoff;
    uint32_t pad01;
    uint32_t pad02;
    glm::vec4 baseColorFactor{};
  };

  struct Material
  {
    GpuMaterial gpuMaterial;
    int baseColorTextureIdx;
  };

  //struct GeometryBuffers
  //{
  //  std::optional<GFX::Buffer> vertexBuffer;
  //  std::optional<GFX::Buffer> indexBuffer;
  //  GFX::IndexType indexType;
  //};

  struct Mesh
  {
    //const GeometryBuffers* buffers;
    std::optional<GFX::Buffer> vertexBuffer;
    std::optional<GFX::Buffer> indexBuffer;
    int materialIdx;
    glm::mat4 transform;
  };

  struct Scene
  {
    std::vector<Mesh> meshes;
    //std::vector<GeometryBuffers> geometry;
    std::vector<Material> materials;
    std::vector<CombinedTextureSampler> textureSamplers;
  };

  bool LoadModelFromFile(Scene& scene, std::string_view fileName, glm::mat4 rootTransform = glm::mat4{ 1 }, bool binary = false);
}
