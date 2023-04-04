#include "SceneLoader.h"
#include <iostream>
#include <bit>
#include <numeric>
#include <execution>
#include <stack>
#include <tuple>
#include <optional>
#include <chrono>
#include <algorithm>
#include <ranges>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include FWOG_OPENGL_HEADER

//#include <glm/gtx/string_cast.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// thanks Microsoft
#ifdef WIN32
#define NOUSER
#define NOMINMAX
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
        return static_cast<double>(std::chrono::duration_cast<microsecond_t>(myclock_t::now() - beg_).count());
      }

    private:
      timepoint_t timepoint_;
    };

    glm::vec2 signNotZero(glm::vec2 v)
    {
      return glm::vec2((v.x >= 0.0f) ? +1.0f : -1.0f, (v.y >= 0.0f) ? +1.0f : -1.0f);
    }

    glm::vec2 float32x3_to_oct(glm::vec3 v)
    {
      glm::vec2 p = glm::vec2{ v.x, v.y } * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
      return (v.z <= 0.0f) ? ((1.0f - glm::abs(glm::vec2{ p.y, p.x })) * signNotZero(p)) : p;
    }

    auto ConvertGlAddressMode(uint32_t wrap) -> Fwog::AddressMode
    {
      switch (wrap)
      {
      case GL_CLAMP_TO_EDGE: return Fwog::AddressMode::CLAMP_TO_EDGE;
      case GL_MIRRORED_REPEAT: return Fwog::AddressMode::MIRRORED_REPEAT;
      case GL_REPEAT: return Fwog::AddressMode::REPEAT;
      default:
        FWOG_UNREACHABLE; return Fwog::AddressMode::REPEAT;
      }
    }

    auto ConvertGlFilterMode(uint32_t filter) -> Fwog::Filter
    {
      switch (filter)
      {
      case GL_LINEAR_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_LINEAR:
        return Fwog::Filter::LINEAR;
      case GL_NEAREST_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST:
        return Fwog::Filter::NEAREST;
      default: FWOG_UNREACHABLE; return Fwog::Filter::LINEAR;
      }
    }

    auto GetGlMipmapFilter(uint32_t minFilter) -> Fwog::Filter
    {
      switch (minFilter)
      {
      case GL_LINEAR_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_LINEAR:
        return Fwog::Filter::LINEAR;
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST:
        return Fwog::Filter::NEAREST;
      case GL_LINEAR: //[[fallthrough]]
      case GL_NEAREST:
        return Fwog::Filter::NONE;
      default: FWOG_UNREACHABLE; return Fwog::Filter::NONE;
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

    bool LoadImageData([[maybe_unused]] tinygltf::Image* image, const int image_idx, std::string* err,
      std::string* warn, int req_width, int req_height,
      const unsigned char* bytes, int size, void* user_data)
    {
      int x, y, comp;
      stbi_info_from_memory(bytes, size, &x, &y, &comp);
      auto* data = reinterpret_cast<std::vector<RawImageData>*>(user_data);
      auto* bytes2 = new unsigned char[size];
      memcpy(bytes2, bytes, size);
      data->emplace_back(RawImageData{image_idx, err, warn, req_width, req_height, bytes2, size});

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
    std::vector<glm::vec2> texcoords;

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
          case GL_BYTE:           AddElements.template operator()<Bvec>(); break;
          case GL_UNSIGNED_BYTE:  AddElements.template operator()<UBvec>(); break;
          case GL_SHORT:          AddElements.template operator()<Svec>(); break;
          case GL_UNSIGNED_SHORT: AddElements.template operator()<USvec>(); break;
          case GL_UNSIGNED_INT:   AddElements.template operator()<UIvec>(); break;
          case GL_FLOAT:          AddElements.template operator()<Fvec>(); break;
          default: FWOG_UNREACHABLE;
          }
          
        }
        else if (accessor.normalized == true) // normalized elements require conversion
        {
          auto AddElementsNorm = [&]<typename Vec>()
          {
            for (size_t i = 0; i < accessor.count; i++)
            {
              // this doesn't actually work exactly for signed types
              attributeBuffer[i] = Fvec(*reinterpret_cast<const Vec*>(buffer.data.data() + totalByteOffset + i * stride)) / Fvec(static_cast<float>(std::numeric_limits<typename Vec::value_type>::max()));
            }
          };

          switch (accessor.componentType)
          {
          //case GL_BYTE:           AddElementsNorm.template operator()<Bvec>(); break;
          case GL_UNSIGNED_BYTE:  AddElementsNorm.template operator()<UBvec>(); break;
          //case GL_SHORT:          AddElementsNorm.template operator()<Svec>(); break;
          case GL_UNSIGNED_SHORT: AddElementsNorm.template operator()<USvec>(); break;
          case GL_UNSIGNED_INT:   AddElementsNorm.template operator()<UIvec>(); break;
          //case GL_FLOAT:          AddElementsNorm.template operator()<Fvec>(); break;
          default: FWOG_UNREACHABLE;
          }
        }
        else
        {
          FWOG_UNREACHABLE;
        }
      };

      if (name == "POSITION")
      {
        FWOG_ASSERT(accessor.type == 3);
        InsertData.template operator()<3>(positions);
      }
      else if (name == "NORMAL")
      {
        FWOG_ASSERT(accessor.type == 3);
        InsertData.template operator()<3>(normals);
      }
      else if (name == "TEXCOORD_0")
      {
        FWOG_ASSERT(accessor.type == 2);
        InsertData.template operator()<2>(texcoords);
      }
      else
      {
        std::cout << "Unsupported attribute: " << name << '\n';
        //FWOG_UNREACHABLE;
      }
    }
    
    texcoords.resize(positions.size()); // TEMP HACK
    FWOG_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size());
    
    std::vector<Vertex> vertices;
    vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++)
    {
      vertices[i] = {
        positions[i],
        glm::packSnorm2x16(float32x3_to_oct(normals[i])),
        texcoords[i]
      };
    }

    return vertices;
  }

  std::vector<index_t> ConvertIndexBufferFormat(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
  {
    int accessorIndex = primitive.indices;
    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    size_t totalByteOffset = accessor.byteOffset + bufferView.byteOffset;
    int stride = accessor.ByteStride(bufferView);
    
    std::vector<index_t> indices;
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
      FWOG_UNREACHABLE;
    }

    return indices;
  }

  std::pair<std::vector<Fwog::Texture>, std::vector<Fwog::Sampler>> LoadTextureSamplers(const tinygltf::Model& model)
  {
    std::vector<Fwog::Texture> textures;
    std::vector<Fwog::Sampler> samplers;

    for (const auto& texture : model.textures)
    {
      const tinygltf::Image& image = model.images[texture.source];

      Fwog::SamplerState samplerState;

      // sampler isn't null
      if (texture.sampler >= 0)
      {
        const tinygltf::Sampler& baseColorSampler = model.samplers[texture.sampler];
        samplerState.addressModeU = ConvertGlAddressMode(baseColorSampler.wrapS);
        samplerState.addressModeV = ConvertGlAddressMode(baseColorSampler.wrapT);
        samplerState.minFilter = ConvertGlFilterMode(baseColorSampler.minFilter);
        samplerState.magFilter = ConvertGlFilterMode(baseColorSampler.magFilter);
        samplerState.mipmapFilter = GetGlMipmapFilter(baseColorSampler.minFilter);
        if (GetGlMipmapFilter(baseColorSampler.minFilter) != Fwog::Filter::NONE)
        {
          samplerState.anisotropy = Fwog::SampleCount::SAMPLES_16;
        }
      }

      auto sampler = Fwog::Sampler(samplerState);

      FWOG_ASSERT(image.component == 4);
      FWOG_ASSERT(image.pixel_type == GL_UNSIGNED_BYTE);
      FWOG_ASSERT(image.bits == 8);

      Fwog::Extent2D dims = { static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height) };

      auto textureData = Fwog::CreateTexture2DMip(
        dims,
        Fwog::Format::R8G8B8A8_UNORM,
        uint32_t(1 + floor(log2(glm::max(dims.width, dims.height)))),
        image.name);

      Fwog::TextureUpdateInfo updateInfo
      {
        .dimension = Fwog::UploadDimension::TWO,
        .level = 0,
        .offset = {},
        .size = { dims.width, dims.height, 1 },
        .format = Fwog::UploadFormat::RGBA,
        .type = Fwog::UploadType::UBYTE,
        .pixels = image.image.data()
      };
      textureData.SubImage(updateInfo);
      textureData.GenMipmaps();
      
      textures.emplace_back(std::move(textureData));
      samplers.emplace_back(std::move(sampler));
    }

    return std::make_pair(std::move(textures), std::move(samplers));
  }

  std::vector<Material> LoadMaterials(const tinygltf::Model& model, int baseTextureSamplerIndex, std::span<const Fwog::Texture> textures, std::span<const Fwog::Sampler> samplers)
  {
    std::vector<Material> materials;

    for (const auto& loaderMaterial : model.materials)
    {
      int baseColorTextureIndex = baseTextureSamplerIndex + loaderMaterial.pbrMetallicRoughness.baseColorTexture.index;
      
      glm::vec4 baseColorFactor{};
      for (int i = 0; i < 4; i++)
      {
        baseColorFactor[i] = static_cast<float>(loaderMaterial.pbrMetallicRoughness.baseColorFactor[i]);
      }

      Material material;

      if (baseColorTextureIndex >= 0)
      {
        material.gpuMaterial.flags |= MaterialFlagBit::HAS_BASE_COLOR_TEXTURE;
        material.albedoTextureSampler = 
        {
          textures[baseColorTextureIndex].CreateFormatView(Fwog::Format::R8G8B8A8_SRGB),
          samplers[baseColorTextureIndex]
        };
      }

      material.gpuMaterial.baseColorFactor = baseColorFactor;
      material.gpuMaterial.alphaCutoff = static_cast<float>(loaderMaterial.alphaCutoff);
      materials.emplace_back(std::move(material));
    }

    return materials;
  }

  // compute the object-space bounding box
  Box3D GetBoundingBox(std::span<const Vertex> vertices)
  {
    glm::vec3 min{ 1e20f };
    glm::vec3 max{};
    for (const auto& vertex : vertices)
    {
      min = glm::min(min, vertex.position);
      max = glm::max(max, vertex.position);
    }

    return Box3D
    {
      .offset = (min + max) / 2.0f,
      .halfExtent = (max - min) / 2.0f
    };
  }

  struct CpuMesh
  {
    std::vector<Vertex> vertices;
    std::vector<index_t> indices;
    uint32_t materialIdx;
    glm::mat4 transform;
  };

  struct LoadModelResult
  {
    std::vector<CpuMesh> meshes;
    std::vector<Material> materials;
    std::vector<Fwog::Texture> textures;
    std::vector<Fwog::Sampler> samplers;
  };

  std::optional<LoadModelResult> LoadModelFromFileBase(std::string_view fileName, 
    glm::mat4 rootTransform, 
    bool binary,
    uint32_t baseMaterialIndex,
    uint32_t baseTextureSamplerIndex)
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
      return std::nullopt;
    }

    // let's not deal with glTFs containing multiple scenes right now
    FWOG_ASSERT(model.scenes.size() == 1);

    auto ms = timer.Elapsed_us() / 1000;
    std::cout << "Loading took " << ms << " ms\n";

    LoadModelResult scene;

    std::tie(scene.textures, scene.samplers) = LoadTextureSamplers(model);

    auto materials = LoadMaterials(model, baseTextureSamplerIndex, scene.textures, scene.samplers);
    std::ranges::move(materials, std::back_inserter(scene.materials));

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

          scene.meshes.emplace_back(CpuMesh
            {
              std::move(vertices),
              std::move(indices),
              baseMaterialIndex + std::max(primitive.material, 0),
              globalTransform
            });
        }
      }
    }

    std::cout << "Loaded glTF: " << fileName << '\n';

    return scene;
  }

  bool LoadModelFromFile(Scene& scene, std::string_view fileName, glm::mat4 rootTransform, bool binary)
  {
    FWOG_ASSERT(scene.textures.size() == scene.samplers.size());
    const auto baseMaterialIndex = static_cast<uint32_t>(scene.materials.size());
    const auto baseTextureSamplerIndex = static_cast<uint32_t>(scene.textures.size());

    auto loadedScene = LoadModelFromFileBase(fileName, rootTransform, binary, baseMaterialIndex, baseTextureSamplerIndex);

    if (!loadedScene)
      return false;

    scene.meshes.reserve(scene.meshes.size() + loadedScene->meshes.size());
    for (auto& mesh : loadedScene->meshes)
    {
      scene.meshes.emplace_back(Mesh
        {
          .vertexBuffer = Fwog::Buffer(std::span(mesh.vertices)),
          .indexBuffer = Fwog::Buffer(std::span(mesh.indices)),
          .materialIdx = mesh.materialIdx,
          .transform = mesh.transform
        });
    }

    std::ranges::move(loadedScene->materials, std::back_inserter(scene.materials));

    std::ranges::move(loadedScene->textures, std::back_inserter(scene.textures));

    std::ranges::move(loadedScene->samplers, std::back_inserter(scene.samplers));

    return true;
  }

  bool LoadModelFromFileBindless(SceneBindless& scene, std::string_view fileName, glm::mat4 rootTransform, bool binary)
  {
    FWOG_ASSERT(scene.textures.size() == scene.samplers.size());
    const auto baseMaterialIndex = static_cast<uint32_t>(scene.materials.size());
    const auto baseTextureSamplerIndex = static_cast<uint32_t>(scene.materials.size());

    auto loadedScene = LoadModelFromFileBase(fileName, rootTransform, binary, baseMaterialIndex, baseTextureSamplerIndex);

    if (!loadedScene)
      return false;

    scene.meshes.reserve(scene.meshes.size() + loadedScene->meshes.size());
    for (auto& mesh : loadedScene->meshes)
    {
      scene.meshes.emplace_back(MeshBindless
        {
          .startVertex = static_cast<int32_t>(scene.vertices.size()),
          .startIndex = static_cast<uint32_t>(scene.indices.size()),
          .indexCount = static_cast<uint32_t>(mesh.indices.size()),
          .materialIdx = mesh.materialIdx,
          .transform = mesh.transform,
          .boundingBox = GetBoundingBox(mesh.vertices)
        });

      std::vector<Vertex> tempVertices = std::move(mesh.vertices);
      scene.vertices.insert(scene.vertices.end(), tempVertices.begin(), tempVertices.end());

      std::vector<index_t> tempIndices = std::move(mesh.indices);
      scene.indices.insert(scene.indices.end(), tempIndices.begin(), tempIndices.end());
    }

    std::ranges::move(loadedScene->textures, std::back_inserter(scene.textures));

    std::ranges::move(loadedScene->samplers, std::back_inserter(scene.samplers));

    scene.materials.reserve(scene.materials.size() + loadedScene->materials.size());
    for (auto& material : loadedScene->materials)
    {
      GpuMaterialBindless bindlessMaterial
      {
        .flags = material.gpuMaterial.flags,
        .alphaCutoff = material.gpuMaterial.alphaCutoff,
        .baseColorTextureHandle = 0,
        .baseColorFactor = material.gpuMaterial.baseColorFactor
      };
      if (material.gpuMaterial.flags & MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
      {
        auto& [texture, sampler] = material.albedoTextureSampler.value();
        bindlessMaterial.baseColorTextureHandle = texture.GetBindlessHandle(sampler);
      }
      scene.materials.emplace_back(bindlessMaterial);
    }

    return true;
  }
}
