#include "SceneLoader.h"
#include <iostream>
#include <bit>
#include <glad/gl.h>
#include <gsdf/Common.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// thanks Microsoft
#ifdef WIN32
#define NOUSER
#endif
#include <tiny_gltf.h>
#ifdef WIN32
#undef MemoryBarrier
#endif

namespace Utility
{
  std::vector<Vertex> ConvertVertexBufferFormat(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
  {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> texcoords;

    for (const auto& [name, accessorIndex] : primitive.attributes)
    {
      std::cout << "Attribute: " << name << ", Accessor: " << accessorIndex << '\n';

      const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
      const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

      size_t totalByteOffset = accessor.byteOffset + bufferView.byteOffset;
      int stride = accessor.ByteStride(bufferView);

      GSDF_ASSERT(accessor.componentType == GL_FLOAT);
      GSDF_ASSERT(accessor.normalized == false);

      auto InsertData = [&]<typename Vec>(std::vector<Vec>& attributeBuffer)
      {
        attributeBuffer.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; i++)
        {
          attributeBuffer[i] = *reinterpret_cast<const glm::vec3*>(buffer.data.data() + totalByteOffset + i * stride);
        }
      };

      if (name == "POSITION")
      {
        GSDF_ASSERT(accessor.type == 3);
        InsertData(positions);
      }
      else if (name == "NORMAL")
      {
        GSDF_ASSERT(accessor.type == 3);
        InsertData(normals);
      }
      else if (name == "TEXCOORD_0")
      {
        GSDF_ASSERT(accessor.type == 2);
        InsertData(texcoords);
      }
      else
      {
        GSDF_UNREACHABLE;
      }
    }
    
    GSDF_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size());
    
    std::vector<Vertex> vertices;
    vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++)
    {
      vertices[i] = { positions[i], normals[i], texcoords[i] };
    }

    return vertices;
  }

  std::vector<uint16_t> ConvertIndexBufferFormat(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
  {
    int accessorIndex = primitive.indices;
    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    size_t totalByteOffset = accessor.byteOffset + bufferView.byteOffset;
    int stride = accessor.ByteStride(bufferView);
    
    std::vector<uint16_t> indices;
    indices.resize(accessor.count);
    
    //if (accessor.componentType == GL_UNSIGNED_INT)
    //{
    //  for (size_t i = 0; i < accessor.count; i++)
    //  {
    //    indices[i] = *reinterpret_cast<const uint32_t*>(buffer.data.data() + totalByteOffset + i * stride);
    //  }
    //}
    if (accessor.componentType == GL_UNSIGNED_SHORT)
    {
      for (size_t i = 0; i < accessor.count; i++)
      {
        indices[i] = *reinterpret_cast<const uint16_t*>(buffer.data.data() + totalByteOffset + i * stride);
      }
    }
    else
    {
      GSDF_UNREACHABLE;
    }

    return indices;
  }

  std::optional<Scene> LoadModelFromFile(std::string_view fileName)
  {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error;
    std::string warning;
    
    bool result = loader.LoadASCIIFromFile(&model, &error, &warning, std::string(fileName));
    if (!warning.empty())
    {
      std::cout << "glTF warning: " << warning << '\n';
    }

    if (!error.empty())
    {
      std::cout << "glTF error: " << error << '\n';
    }

    if (result == false)
    {
      std::cout << "Failed to load glTF: " << fileName << '\n';
      return std::nullopt;
    }

    Scene scene;

    for (const auto& mesh : model.meshes)
    {
      for (const auto& primitive : mesh.primitives)
      {
        auto vertices = ConvertVertexBufferFormat(model, primitive);
        auto indices = ConvertIndexBufferFormat(model, primitive);

        auto vertexBuffer = GFX::Buffer::Create(std::span(vertices));
        auto indexBuffer = GFX::Buffer::Create(std::span(indices));
        scene.meshes.emplace_back(std::move(vertexBuffer), std::move(indexBuffer));
      }
    }

    std::cout << "Loaded glTF: " << fileName << '\n';

    return scene;
  }
}
