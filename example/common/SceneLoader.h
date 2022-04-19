#pragma once
#include <vector>
#include <optional>
#include <string_view>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <gsdf/Buffer.h>

namespace Utility
{
  struct Vertex
  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
  };

  struct Mesh
  {
    std::optional<GFX::Buffer> vertexBuffer;
    std::optional<GFX::Buffer> indexBuffer;
  };

  //struct MeshInstance
  //{
  //  glm::mat4 transform;
  //  const Mesh* mesh;
  //};

  struct Scene
  {
    std::vector<Mesh> meshes;
  };

  std::optional<Scene> LoadModelFromFile(std::string_view fileName);
}
