#pragma once
#include <vector>
#include <string_view>
#include <optional>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <Fwog/detail/Flags.h>
#include <Fwog/Buffer.h>
#include <Fwog/Texture.h>

namespace Utility
{
  struct Vertex
  {
    glm::vec3 position;
    uint32_t normal;
    glm::vec2 texcoord;
  };

  using index_t = uint32_t;

  struct Box3D
  {
    glm::vec3 offset;
    glm::vec3 halfExtent;
  };

  struct CombinedTextureSampler
  {
    Fwog::TextureView texture;
    Fwog::Sampler sampler;
  };

  enum class MaterialFlagBit
  {
    HAS_BASE_COLOR_TEXTURE = 1 << 0,
  };
  FWOG_DECLARE_FLAG_TYPE(MaterialFlags, MaterialFlagBit, uint32_t)

  struct GpuMaterial
  {
    MaterialFlags flags{};
    float alphaCutoff{};
    uint32_t pad01{};
    uint32_t pad02{};
    glm::vec4 baseColorFactor{};
  };

  struct GpuMaterialBindless
  {
    MaterialFlags flags{};
    float alphaCutoff{};
    uint64_t baseColorTextureHandle{};
    glm::vec4 baseColorFactor{};
  };

  struct Material
  {
    GpuMaterial gpuMaterial{};
    std::optional<CombinedTextureSampler> albedoTextureSampler;
  };

  struct Mesh
  {
    //const GeometryBuffers* buffers;
    Fwog::Buffer vertexBuffer;
    Fwog::Buffer indexBuffer;
    uint32_t materialIdx{};
    glm::mat4 transform{};
  };

  struct Scene
  {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Fwog::Texture> textures;
    std::vector<Fwog::Sampler> samplers;
  };

  struct MeshBindless
  {
    int32_t startVertex{};
    uint32_t startIndex{};
    uint32_t indexCount{};
    uint32_t materialIdx{};
    glm::mat4 transform{};
    Box3D boundingBox{};
  };

  struct SceneBindless
  {
    std::vector<MeshBindless> meshes;
    std::vector<Vertex> vertices;
    std::vector<index_t> indices;
    std::vector<GpuMaterialBindless> materials;
    std::vector<Fwog::Texture> textures;
    std::vector<Fwog::Sampler> samplers;
  };

  bool LoadModelFromFile(Scene& scene, 
    std::string_view fileName, 
    glm::mat4 rootTransform = glm::mat4{ 1 }, 
    bool binary = false);

  bool LoadModelFromFileBindless(SceneBindless& scene, 
    std::string_view fileName, 
    glm::mat4 rootTransform = glm::mat4{ 1 }, 
    bool binary = false);
}