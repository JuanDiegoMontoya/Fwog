#include "SceneLoader.h"
#include <iostream>
#include <numeric>
#include <execution>
#include <stack>
#include <tuple>
#include <optional>
#include <chrono>
#include <algorithm>
#include <ranges>
#include <span>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "ktx.h"

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

    // Converts a Vulkan BCn VkFormat name to Fwog
    Fwog::Format VkBcFormatToFwog(uint32_t vkFormat)
    {
      // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFormat.html
      switch (vkFormat)
      {
      case 131: return Fwog::Format::BC1_RGB_UNORM;
      case 132: return Fwog::Format::BC1_RGB_SRGB;
      case 133: return Fwog::Format::BC1_RGBA_UNORM;
      case 134: return Fwog::Format::BC1_RGBA_SRGB;
      case 135: return Fwog::Format::BC2_RGBA_UNORM;
      case 136: return Fwog::Format::BC2_RGBA_SRGB;
      case 137: return Fwog::Format::BC3_RGBA_UNORM;
      case 138: return Fwog::Format::BC3_RGBA_SRGB;
      case 139: return Fwog::Format::BC4_R_UNORM;
      case 140: return Fwog::Format::BC4_R_SNORM;
      case 141: return Fwog::Format::BC5_RG_UNORM;
      case 142: return Fwog::Format::BC5_RG_SNORM;
      case 143: return Fwog::Format::BC6H_RGB_UFLOAT;
      case 144: return Fwog::Format::BC6H_RGB_SFLOAT;
      case 145: return Fwog::Format::BC7_RGBA_UNORM;
      case 146: return Fwog::Format::BC7_RGBA_SRGB;
      default: FWOG_UNREACHABLE; return {};
      }
    }

    // Converts a format to the sRGB version of itself, for use in a texture view
    Fwog::Format FormatToSrgb(Fwog::Format format)
    {
      switch (format)
      {
      case Fwog::Format::BC1_RGBA_UNORM: return Fwog::Format::BC1_RGBA_SRGB;
      case Fwog::Format::BC1_RGB_UNORM: return Fwog::Format::BC1_RGB_SRGB;
      case Fwog::Format::BC2_RGBA_UNORM: return Fwog::Format::BC3_RGBA_SRGB;
      case Fwog::Format::BC3_RGBA_UNORM: return Fwog::Format::BC3_RGBA_SRGB;
      case Fwog::Format::BC7_RGBA_UNORM: return Fwog::Format::BC7_RGBA_SRGB;
      case Fwog::Format::R8G8B8A8_UNORM: return Fwog::Format::R8G8B8A8_SRGB;
      case Fwog::Format::R8G8B8_UNORM: return Fwog::Format::R8G8B8_SRGB;
      default: return format;
      }
    }

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

    auto LoadCompressedTexture(const tinygltf::Image& image) -> Fwog::Texture
    {
      return Fwog::Texture(Fwog::TextureCreateInfo{});
    }

    struct RawImageData
    {
      // Used for ktx and non-ktx images alike
      std::unique_ptr<unsigned char[]> encodedPixelData = {};
      size_t encodedPixelSize = 0;

      bool isKtx = false;
      int width = 0;
      int height = 0;
      int pixel_type = GL_UNSIGNED_BYTE;
      int bits = 8;
      int components = 0;
      std::string name;

      // Non-ktx. Raw decoded pixel data
      std::unique_ptr<unsigned char[]> data = {};

      // ktx
      std::unique_ptr<ktxTexture2, decltype([](ktxTexture2* p) { ktxTexture_Destroy(ktxTexture(p)); })> ktx = {};

      //std::optional<Fwog::Texture> texture;
    };

    bool LoadImageData(
      tinygltf::Image* image,
      [[maybe_unused]] const int image_idx,
      [[maybe_unused]] std::string* err,
      [[maybe_unused]] std::string* warn,
      [[maybe_unused]] int req_width,
      [[maybe_unused]] int req_height,
      const unsigned char* bytes,
      int size,
      void* user_data)
    {
      auto* data = static_cast<std::vector<RawImageData>*>(user_data);
      FWOG_ASSERT(image_idx == data->size()); // Assume that traversed image indices are monotonically increasing (0, 1, 2, ..., n - 1)

      auto encodedPixelData = std::make_unique<unsigned char[]>(size);
      memcpy(encodedPixelData.get(), bytes, size);

      data->emplace_back(RawImageData{
        .encodedPixelData = std::move(encodedPixelData),
        .encodedPixelSize = static_cast<size_t>(size),
        .isKtx = image->mimeType == "image/ktx2",
        .name = image->name,
      });

      // This function cannot fail early, since we aren't actually loading the image here
      return true;
    }

    bool LoadImageDataParallel(std::span<RawImageData> rawImageData)
    {
      return std::all_of(std::execution::seq, rawImageData.begin(), rawImageData.end(), [&](RawImageData& rawImage)
      {
        if (rawImage.isKtx)
        {
          ktxTexture2* ktx{};
          if (auto result = ktxTexture2_CreateFromMemory(rawImage.encodedPixelData.get(),
                                                         rawImage.encodedPixelSize,
                                                         KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                         &ktx); result != KTX_SUCCESS)
          {
            return false;
          }
          
          rawImage.width = ktx->baseWidth;
          rawImage.height = ktx->baseHeight;
          rawImage.components = ktxTexture2_GetNumComponents(ktx);
          rawImage.ktx.reset(ktx);
        }
        else
        {
          int x, y, comp;
          auto* pixels = stbi_load_from_memory(rawImage.encodedPixelData.get(), rawImage.encodedPixelSize, &x, &y, &comp, 4);
          if (!pixels)
          {
            return false;
          }

          rawImage.width = x;
          rawImage.height = y;
          //rawImage.components = comp;
          rawImage.components = 4; // If forced 4 components
          rawImage.data.reset(pixels);
        }

        return true;
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
          auto AddElementsNorm = [&]<typename Vec, bool Signed>()
          {
            for (size_t i = 0; i < accessor.count; i++)
            {
              // Unsigned: c / (2^b - 1)
              // Signed: max(c / (2^(b-1) - 1), -1.0)
              const auto vec = Fvec(*reinterpret_cast<const Vec*>(buffer.data.data() + totalByteOffset + i * stride));
              constexpr auto divisor = Fvec(static_cast<float>(std::numeric_limits<typename Vec::value_type>::max()));
              if constexpr (Signed)
              {
                attributeBuffer[i] = glm::max(vec / divisor, Fvec(-1));
              }
              else
              {
                attributeBuffer[i] = vec / divisor;
              }
            }
          };

          switch (accessor.componentType)
          {
          case GL_BYTE:           AddElementsNorm.template operator()<Bvec, true>(); break;
          case GL_UNSIGNED_BYTE:  AddElementsNorm.template operator()<UBvec, false>(); break;
          case GL_SHORT:          AddElementsNorm.template operator()<Svec, true>(); break;
          case GL_UNSIGNED_SHORT: AddElementsNorm.template operator()<USvec, false>(); break;
          case GL_UNSIGNED_INT:   AddElementsNorm.template operator()<UIvec, false>(); break;
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

  std::pair<std::vector<Fwog::Texture>, std::vector<Fwog::SamplerState>> LoadTextureSamplers(const tinygltf::Model& model, std::span<const RawImageData> images)
  {
    std::vector<Fwog::Texture> textures;
    std::vector<Fwog::SamplerState> samplers;

    for (const auto& texture : model.textures)
    {
      int textureSource = texture.source;
      if (auto it = texture.extensions.find("KHR_texture_basisu"); it != texture.extensions.end())
      {
        struct Hack : tinygltf::Value
        {
          Hack(const tinygltf::Value& basisuExtension)
            : Value(basisuExtension) {}

          int GetTextureSource() const
          {
            return this->object_value_.at("source").GetNumberAsInt();
          }
        };
        textureSource = Hack {it->second}.GetTextureSource();
        //printf("source: %d\n", textureSource);
      }
      const auto& image = images[textureSource];

      // Load sampler
      {
        Fwog::SamplerState samplerState{};

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

        samplers.emplace_back(samplerState);
      }

      // Load texture
      {

        Fwog::Extent2D dims = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)};

        if (image.isKtx)
        {
          auto* ktx = image.ktx.get();

          Fwog::Format format = Fwog::Format::BC7_RGBA_UNORM;

          // If the image needs is in a supercompressed encoding, transcode it to a desired format
          if (ktxTexture2_NeedsTranscoding(ktx))
          {
            if (auto result = ktxTexture2_TranscodeBasis(ktx, KTX_TTF_BC7_RGBA, KTX_TF_HIGH_QUALITY); result != KTX_SUCCESS)
            {
              FWOG_UNREACHABLE;
            }
          }
          else
          {
            // Use the format that the image is already in
            format = VkBcFormatToFwog(ktx->vkFormat);
          }

          auto textureData = Fwog::CreateTexture2DMip(dims, format, ktx->numLevels, image.name);

          for (uint32_t level = 0; level < ktx->numLevels; level++)
          {
            size_t offset{};
            ktxTexture_GetImageOffset(ktxTexture(ktx), level, 0, 0, &offset);

            uint32_t width = std::max(dims.width >> level, 1u);
            uint32_t height = std::max(dims.height >> level, 1u);

            textureData.UpdateCompressedImage({
              .level = level,
              .extent = {width, height, 1},
              .data = ktx->pData + offset,
            });
          }

          textures.emplace_back(std::move(textureData));
        }
        else
        {
          FWOG_ASSERT(image.components == 4);
          FWOG_ASSERT(image.pixel_type == GL_UNSIGNED_BYTE);
          FWOG_ASSERT(image.bits == 8);

          auto textureData = Fwog::CreateTexture2DMip(dims,
                                                      Fwog::Format::R8G8B8A8_UNORM,
                                                      uint32_t(1 + floor(log2(glm::max(dims.width, dims.height)))),
                                                      image.name);

          Fwog::TextureUpdateInfo updateInfo{.level = 0,
                                             .offset = {},
                                             .extent = {dims.width, dims.height, 1},
                                             .format = Fwog::UploadFormat::RGBA,
                                             .type = Fwog::UploadType::UBYTE,
                                             .pixels = image.data.get()};
          textureData.UpdateImage(updateInfo);
          textureData.GenMipmaps();

          textures.emplace_back(std::move(textureData));
        }
      }
    }

    return std::make_pair(std::move(textures), std::move(samplers));
  }

  std::vector<Material> LoadMaterials(const tinygltf::Model& model, int baseTextureSamplerIndex, std::span<Fwog::Texture> textures, std::span<const Fwog::SamplerState> samplers)
  {
    std::vector<Material> materials;

    for (const auto& loaderMaterial : model.materials)
    {
      int baseColorTextureIndex = loaderMaterial.pbrMetallicRoughness.baseColorTexture.index;
      
      glm::vec4 baseColorFactor{};
      for (int i = 0; i < 4; i++)
      {
        baseColorFactor[i] = static_cast<float>(loaderMaterial.pbrMetallicRoughness.baseColorFactor[i]);
      }

      Material material;

      if (baseColorTextureIndex >= 0)
      {
        auto& tex = textures[baseColorTextureIndex];
        material.gpuMaterial.flags |= MaterialFlagBit::HAS_BASE_COLOR_TEXTURE;
        material.albedoTextureSampler = 
        {
          tex.CreateFormatView(FormatToSrgb(tex.GetCreateInfo().format)),
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
    std::vector<Fwog::SamplerState> samplers;
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

    if (std::ranges::find(model.extensionsUsed, "KHR_texture_transform") != model.extensionsUsed.end())
    {
      std::cout << "glTF contains unsupported extension: "
                   "KHR_texture_transform\n";
      result = false;
    }

    if (result == false)
    {
      std::cout << "Failed to load glTF: " << fileName << '\n';
      return std::nullopt;
    }

    bool loadImageResult = LoadImageDataParallel(rawImageData);
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

    std::tie(scene.textures, scene.samplers) = LoadTextureSamplers(model, rawImageData);

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

      //std::cout << "Node: " << node->name << '\n';

      glm::mat4 localTransform = NodeToMat4(*node);
      glm::mat4 globalTransform = parentGlobalTransform * localTransform;

      for (int childNodeIndex : node->children)
      {
        nodeStack.emplace(&model.nodes[childNodeIndex], globalTransform);
      }

      if (node->mesh >= 0)
      {
        // TODO: get a reference to the mesh instead of loading it from scratch
        for (const tinygltf::Mesh& mesh = model.meshes[node->mesh]; const auto& primitive : mesh.primitives)
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
        bindlessMaterial.baseColorTextureHandle = texture.GetBindlessHandle(Fwog::Sampler(sampler));
      }
      scene.materials.emplace_back(bindlessMaterial);
    }

    return true;
  }
}
