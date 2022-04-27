#include "SceneLoader.h"
#include <iostream>
#include <bit>
#include <numeric>
#include <execution>
#include <stack>
#include <tuple>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glad/gl.h>
#include <gsdf/Common.h>

#include <glm/gtx/string_cast.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// thanks Microsoft
#ifdef WIN32
#define NOUSER
#endif
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_USE_CPP14
#include <tiny_gltf.h>

namespace Utility
{
  namespace // helpers
  {
    class Timer
    {
      using microsecond_t = std::chrono::microseconds;
      using myclock_t = std::chrono::high_resolution_clock;
      using timepoint_t = std::chrono::time_point<myclock_t>;
    public:
      Timer()
      {
        timepoint_ = myclock_t::now();
      }

      void Reset()
      {
        timepoint_ = myclock_t::now();
      }

      double Elapsed_us() const
      {
        timepoint_t beg_ = timepoint_;
        return std::chrono::duration_cast<microsecond_t>(myclock_t::now() - beg_).count();
      }

    private:
      timepoint_t timepoint_;
    };

    auto ConvertGlAddressMode(uint32_t wrap) -> GFX::AddressMode
    {
      switch (wrap)
      {
      case GL_CLAMP_TO_EDGE: return GFX::AddressMode::CLAMP_TO_EDGE;
      case GL_MIRRORED_REPEAT: return GFX::AddressMode::MIRRORED_REPEAT;
      case GL_REPEAT: return GFX::AddressMode::REPEAT;
      default:
        GSDF_UNREACHABLE; return GFX::AddressMode::REPEAT;
      }
    }

    auto ConvertGlFilterMode(uint32_t filter) -> GFX::Filter
    {
      switch (filter)
      {
      case GL_LINEAR_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_LINEAR:
        return GFX::Filter::LINEAR;
      case GL_NEAREST_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST:
        return GFX::Filter::NEAREST;
      default: GSDF_UNREACHABLE; return GFX::Filter::LINEAR;
      }
    }

    auto GetGlMipmapFilter(uint32_t minFilter) -> GFX::Filter
    {
      switch (minFilter)
      {
      case GL_LINEAR_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_LINEAR:
        return GFX::Filter::LINEAR;
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST:
        return GFX::Filter::NEAREST;
      case GL_LINEAR: //[[fallthrough]]
      case GL_NEAREST:
        return GFX::Filter::NONE;
      default: GSDF_UNREACHABLE; return GFX::Filter::NONE;
      }
    }

    struct RawImageData
    {
      const int image_idx;
      std::string* err;
      std::string* warn;
      int req_width;
      int req_height;
      const unsigned char* bytes;
      int size;
    };

    bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
      std::string* warn, int req_width, int req_height,
      const unsigned char* bytes, int size, void* user_data)
    {
      int x, y, comp;
      auto ret = stbi_info_from_memory(bytes, size, &x, &y, &comp);
      auto* data = reinterpret_cast<std::vector<RawImageData>*>(user_data);
      auto* bytes2 = new unsigned char[size];
      memcpy(bytes2, bytes, size);
      data->emplace_back(image_idx, err, warn, req_width, req_height, bytes2, size);

      // TODO: this should return false if the image is unloadable (but how?)
      return true;
    }

    bool LoadImageDataParallel(std::vector<RawImageData>& rawImageData, std::vector<tinygltf::Image>& images, tinygltf::LoadImageDataOption options)
    {
      return std::all_of(std::execution::par, rawImageData.begin(), rawImageData.end(), [&](RawImageData& rawImage) mutable
        {
          auto& [image_idx, err, warn, req_width, req_height, bytes, size] = rawImage;
          bool success = tinygltf::LoadImageData(&images[image_idx], image_idx, err, warn, req_width, req_height, bytes, size, &options);
          delete[] bytes;
          return success;
        });
    }

    glm::mat4 NodeToMat4(const tinygltf::Node& node)
    {
      glm::mat4 transform{ 1 };

      if (node.matrix.empty())
      {
        glm::quat rotation{ 1, 0, 0, 0 }; // wxyz
        glm::vec3 scale{ 1 };
        glm::vec3 translation{ 0 };

        if (node.rotation.size() == 4)
        {
          const auto& q = node.rotation;
          rotation = glm::dquat{ q[3], q[0], q[1], q[2] };
        }

        if (node.scale.size() == 3)
        {
          const auto& s = node.scale;
          scale = glm::vec3{ s[0], s[1], s[2] };
        }

        if (node.translation.size() == 3)
        {
          const auto& t = node.translation;
          translation = glm::vec3{ t[0], t[1], t[2] };
        }

        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 translationMat = glm::translate(translation);
        glm::mat4 scaleMat = glm::scale(scale);

        // T * R * S
        transform = glm::scale(glm::translate(translation) * rotationMat, scale);
      }
      else if (node.matrix.size() == 16)
      {
        const auto& m = node.matrix;
        transform = { 
          m[0], m[1], m[2], m[3], 
          m[4], m[5], m[6], m[7], 
          m[8], m[9], m[10], m[11], 
          m[12], m[13], m[14], m[15] };
      }
      // else node has identity transform

      return transform;
    }
  }

  std::vector<Vertex> ConvertVertexBufferFormat(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
  {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> texcoords;

    for (const auto& [name, accessorIndex] : primitive.attributes)
    {
      //std::cout << "Attribute: " << name << ", Accessor: " << accessorIndex << '\n';

      const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
      const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

      size_t totalByteOffset = accessor.byteOffset + bufferView.byteOffset;
      int stride = accessor.ByteStride(bufferView);

      auto InsertData = [&]<size_t N>(std::vector<glm::vec<N, float>>& attributeBuffer)
      {
        using Fvec = glm::vec<N, float>;
        using Bvec = glm::vec<N, int8_t>;
        using UBvec = glm::vec<N, uint8_t>;
        using Svec = glm::vec<N, int16_t>;
        using USvec = glm::vec<N, uint16_t>;
        using UIvec = glm::vec<N, uint32_t>;
        Fvec::length();

        attributeBuffer.resize(accessor.count);

        if (stride == sizeof(Fvec) && accessor.componentType == GL_FLOAT && accessor.normalized == false)
        {
          memcpy(attributeBuffer.data(), buffer.data.data() + totalByteOffset, sizeof(Fvec) * accessor.count);
        }
        else if (accessor.normalized == false)
        {
          auto AddElements = [&]<typename Vec>()
          {
            for (size_t i = 0; i < accessor.count; i++)
            {
              attributeBuffer[i] = *reinterpret_cast<const Vec*>(buffer.data.data() + totalByteOffset + i * stride);
            }
          };

          switch (accessor.componentType)
          {
          case GL_BYTE:           AddElements.operator()<Bvec>(); break;
          case GL_UNSIGNED_BYTE:  AddElements.operator()<UBvec>(); break;
          case GL_SHORT:          AddElements.operator()<Svec>(); break;
          case GL_UNSIGNED_SHORT: AddElements.operator()<USvec>(); break;
          case GL_UNSIGNED_INT:   AddElements.operator()<UIvec>(); break;
          case GL_FLOAT:          AddElements.operator()<Fvec>(); break;
          default: GSDF_UNREACHABLE;
          }
          
        }
        else if (accessor.normalized == true) // normalized elements require conversion
        {
          auto AddElementsNorm = [&]<typename Vec>()
          {
            for (size_t i = 0; i < accessor.count; i++)
            {
              // this doesn't actually work exactly for signed types
              attributeBuffer[i] = Fvec(*reinterpret_cast<const Vec*>(buffer.data.data() + totalByteOffset + i * stride)) / Fvec(std::numeric_limits<Vec::value_type>::max());
            }
          };

          switch (accessor.componentType)
          {
          //case GL_BYTE:           AddElementsNorm.operator()<Bvec>(); break;
          case GL_UNSIGNED_BYTE:  AddElementsNorm.operator()<UBvec>(); break;
          //case GL_SHORT:          AddElementsNorm.operator()<Svec>(); break;
          case GL_UNSIGNED_SHORT: AddElementsNorm.operator()<USvec>(); break;
          case GL_UNSIGNED_INT:   AddElementsNorm.operator()<UIvec>(); break;
          //case GL_FLOAT:          AddElementsNorm.operator()<Fvec>(); break;
          default: GSDF_UNREACHABLE;
          }
        }
        else
        {
          GSDF_UNREACHABLE;
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
        std::cout << "Unsupported attribute: " << name << '\n';
        //GSDF_UNREACHABLE;
      }
    }
    
    texcoords.resize(positions.size()); // TEMP HACK
    GSDF_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size());
    
    std::vector<Vertex> vertices;
    vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++)
    {
      vertices[i] = { positions[i], normals[i], texcoords[i] };
    }

    return vertices;
  }

  std::vector<uint32_t> ConvertIndexBufferFormat(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
  {
    int accessorIndex = primitive.indices;
    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    size_t totalByteOffset = accessor.byteOffset + bufferView.byteOffset;
    int stride = accessor.ByteStride(bufferView);
    
    std::vector<uint32_t> indices;
    indices.resize(accessor.count);
    
    if (accessor.componentType == GL_UNSIGNED_INT)
    {
      for (size_t i = 0; i < accessor.count; i++)
      {
        indices[i] = *reinterpret_cast<const uint32_t*>(buffer.data.data() + totalByteOffset + i * stride);
      }
    }
    else if (accessor.componentType == GL_UNSIGNED_SHORT)
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

  std::vector<CombinedTextureSampler> LoadTextureSamplers(const tinygltf::Model& model)
  {
    std::vector<CombinedTextureSampler> textureSamplers;

    for (const auto& texture : model.textures)
    {
      const tinygltf::Image& image = model.images[texture.source];

      GFX::SamplerState samplerState;

      // sampler isn't null
      if (texture.sampler >= 0)
      {
        const tinygltf::Sampler& baseColorSampler = model.samplers[texture.sampler];
        samplerState.addressModeU = ConvertGlAddressMode(baseColorSampler.wrapS);
        samplerState.addressModeV = ConvertGlAddressMode(baseColorSampler.wrapT);
        samplerState.minFilter = ConvertGlFilterMode(baseColorSampler.minFilter);
        samplerState.magFilter = ConvertGlFilterMode(baseColorSampler.magFilter);
        samplerState.mipmapFilter = GetGlMipmapFilter(baseColorSampler.minFilter);
      }

      auto sampler = GFX::TextureSampler::Create(samplerState);

      GSDF_ASSERT(image.component == 4);
      GSDF_ASSERT(image.pixel_type == GL_UNSIGNED_BYTE);
      GSDF_ASSERT(image.bits == 8);

      GFX::Extent2D dims = { static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height) };

      auto textureData = GFX::CreateTexture2D(dims, GFX::Format::R8G8B8A8_SRGB, image.name);

      GFX::TextureUpdateInfo updateInfo
      {
        .dimension = GFX::UploadDimension::TWO,
        .level = 0,
        .offset = {},
        .size = { dims.width, dims.height, 1 },
        .format = GFX::UploadFormat::RGBA,
        .type = GFX::UploadType::UBYTE,
        .pixels = image.image.data()
      };
      textureData->SubImage(updateInfo);

      auto view = textureData->View();

      textureSamplers.emplace_back(CombinedTextureSampler({ std::move(textureData), std::move(view), std::move(sampler) }));
    }

    return textureSamplers;
  }

  std::vector<Material> LoadMaterials(const tinygltf::Model& model, const std::vector<CombinedTextureSampler>& textureSamplers)
  {
    std::vector<Material> materials;

    for (const auto& loaderMaterial : model.materials)
    {
      int baseColorTextureIndex = loaderMaterial.pbrMetallicRoughness.baseColorTexture.index;
      
      glm::vec4 baseColorFactor{};
      for (int i = 0; i < 4; i++)
      {
        baseColorFactor[i] = loaderMaterial.pbrMetallicRoughness.baseColorFactor[i];
      }

      Material material;

      if (baseColorTextureIndex >= 0)
      {
        material.gpuMaterial.flags |= MaterialFlagBit::HAS_BASE_COLOR_TEXTURE;
        material.baseColorTexture = &textureSamplers[baseColorTextureIndex];
      }

      material.gpuMaterial.baseColorFactor = baseColorFactor;
      material.gpuMaterial.alphaCutoff = loaderMaterial.alphaCutoff;
      materials.emplace_back(material);
    }

    return materials;
  }

  std::optional<Scene> LoadModelFromFile(std::string_view fileName, glm::mat4 rootTransform, bool binary)
  {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error;
    std::string warning;

    Timer timer;

    std::vector<RawImageData> rawImageData;
    loader.SetImageLoader(LoadImageData, &rawImageData);
    
    bool result;
    if (binary)
    {
      result = loader.LoadBinaryFromFile(&model, &error, &warning, std::string(fileName));
    }
    else
    {
      result = loader.LoadASCIIFromFile(&model, &error, &warning, std::string(fileName));
    }

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

    bool loadImageResult = LoadImageDataParallel(rawImageData, model.images, { .preserve_channels = false });
    if (loadImageResult == false)
    {
      std::cout << "Failed to load glTF images" << '\n';
    }

    auto ms = timer.Elapsed_us() / 1000;
    std::cout << "Loading took " << ms << " ms\n";

    Scene scene;

    scene.textureSamplers = LoadTextureSamplers(model);
    scene.materials = LoadMaterials(model, scene.textureSamplers);
    
    GSDF_ASSERT(!model.scenes.empty());

    // <node*, global transform>
    std::stack<std::pair<const tinygltf::Node*, glm::mat4>> nodeStack;
    
    for (int nodeIndex : model.scenes[0].nodes)
    {
      nodeStack.emplace(&model.nodes[nodeIndex], rootTransform);
    }

    while (!nodeStack.empty())
    {
      decltype(nodeStack)::value_type top = nodeStack.top();
      const auto& [node, parentGlobalTransform] = top;
      nodeStack.pop();

      std::cout << "Node: " << node->name << '\n';

      glm::mat4 localTransform = NodeToMat4(*node);
      glm::mat4 globalTransform = parentGlobalTransform * localTransform;

      for (int childNodeIndex : node->children)
      {
        nodeStack.emplace(&model.nodes[childNodeIndex], globalTransform);
      }

      if (node->mesh >= 0)
      {
        // TODO: get a reference to the mesh instead of loading it from scratch
        const tinygltf::Mesh& mesh = model.meshes[node->mesh];
        for (const auto& primitive : mesh.primitives)
        {
          auto vertices = ConvertVertexBufferFormat(model, primitive);
          auto indices = ConvertIndexBufferFormat(model, primitive);

          auto vertexBuffer = GFX::Buffer::Create(std::span(vertices));
          auto indexBuffer = GFX::Buffer::Create(std::span(indices));

          scene.meshes.emplace_back(Mesh
            {
              std::move(vertexBuffer),
              std::move(indexBuffer),
              &scene.materials[std::max(primitive.material, 0)],
              globalTransform
            });
        }
      }
    }

    // TODO: use this code
    //for (const auto& mesh : model.meshes)
    //{
    //  for (const auto& primitive : mesh.primitives)
    //  {
    //    auto vertices = ConvertVertexBufferFormat(model, primitive);
    //    auto indices = ConvertIndexBufferFormat(model, primitive);

    //    auto vertexBuffer = GFX::Buffer::Create(std::span(vertices));
    //    auto indexBuffer = GFX::Buffer::Create(std::span(indices));
    //    
    //    scene.meshes.emplace_back(Mesh
    //      {
    //        std::move(vertexBuffer),
    //        std::move(indexBuffer),
    //        &scene.materials[primitive.material]
    //      });
    //  }
    //}

    std::cout << "Loaded glTF: " << fileName << '\n';

    return scene;
  }
}
