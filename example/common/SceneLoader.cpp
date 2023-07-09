#include "SceneLoader.h"
#include "Application.h"

#include <algorithm>
#include <chrono>
#include <execution>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stack>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "ktx.h"

#include FWOG_OPENGL_HEADER

// #include <glm/gtx/string_cast.hpp>

#include <stb_image.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

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
      glm::vec2 p = glm::vec2{v.x, v.y} * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
      return (v.z <= 0.0f) ? ((1.0f - glm::abs(glm::vec2{p.y, p.x})) * signNotZero(p)) : p;
    }

    auto ConvertGlAddressMode(uint32_t wrap) -> Fwog::AddressMode
    {
      switch (wrap)
      {
      case GL_CLAMP_TO_EDGE: return Fwog::AddressMode::CLAMP_TO_EDGE;
      case GL_MIRRORED_REPEAT: return Fwog::AddressMode::MIRRORED_REPEAT;
      case GL_REPEAT: return Fwog::AddressMode::REPEAT;
      default: FWOG_UNREACHABLE; return Fwog::AddressMode::REPEAT;
      }
    }

    auto ConvertGlFilterMode(uint32_t filter) -> Fwog::Filter
    {
      switch (filter)
      {
      case GL_LINEAR_MIPMAP_LINEAR:  //[[fallthrough]]
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_LINEAR: return Fwog::Filter::LINEAR;
      case GL_NEAREST_MIPMAP_LINEAR:  //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST: return Fwog::Filter::NEAREST;
      default: FWOG_UNREACHABLE; return Fwog::Filter::LINEAR;
      }
    }

    auto GetGlMipmapFilter(uint32_t minFilter) -> Fwog::Filter
    {
      switch (minFilter)
      {
      case GL_LINEAR_MIPMAP_LINEAR: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_LINEAR: return Fwog::Filter::LINEAR;
      case GL_LINEAR_MIPMAP_NEAREST: //[[fallthrough]]
      case GL_NEAREST_MIPMAP_NEAREST: return Fwog::Filter::NEAREST;
      case GL_LINEAR: //[[fallthrough]]
      case GL_NEAREST: return Fwog::Filter::NONE;
      default: FWOG_UNREACHABLE; return Fwog::Filter::NONE;
      }
    }

    std::vector<Fwog::Texture> LoadImages(const fastgltf::Asset& asset)
    {
      struct RawImageData
      {
        // Used for ktx and non-ktx images alike
        std::unique_ptr<std::byte[]> encodedPixelData = {};
        std::size_t encodedPixelSize = 0;

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
      };

      auto MakeRawImageData =
        [](const void* data, std::size_t dataSize, fastgltf::MimeType mimeType, std::string_view name) -> RawImageData
      {
        FWOG_ASSERT(mimeType == fastgltf::MimeType::JPEG || mimeType == fastgltf::MimeType::PNG ||
                    mimeType == fastgltf::MimeType::KTX2);
        auto dataCopy = std::make_unique<std::byte[]>(dataSize);
        std::copy_n(static_cast<const std::byte*>(data), dataSize, dataCopy.get());

        return RawImageData{
          .encodedPixelData = std::move(dataCopy),
          .encodedPixelSize = dataSize,
          .isKtx = mimeType == fastgltf::MimeType::KTX2,
          .name = std::string(name),
        };
      };

      // Load and decode image data locally, in parallel
      auto rawImageData = std::vector<RawImageData>(asset.images.size());

      std::transform(
        std::execution::par,
        asset.images.begin(),
        asset.images.end(),
        rawImageData.begin(),
        [&](const fastgltf::Image& image)
        {
          auto rawImage = [&]
          {
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data))
            {
              FWOG_ASSERT(filePath->fileByteOffset == 0); // We don't support file offsets
              FWOG_ASSERT(filePath->uri.isLocalPath());   // We're only capable of loading local files

              auto fileData = Application::LoadBinaryFile(filePath->uri.path());

              return MakeRawImageData(fileData.first.get(), fileData.second, filePath->mimeType, image.name);
            }

            if (const auto* vector = std::get_if<fastgltf::sources::Vector>(&image.data))
            {
              return MakeRawImageData(vector->bytes.data(), vector->bytes.size(), vector->mimeType, image.name);
            }

            if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data))
            {
              auto& bufferView = asset.bufferViews[view->bufferViewIndex];
              auto& buffer = asset.buffers[bufferView.bufferIndex];
              if (const auto* vector = std::get_if<fastgltf::sources::Vector>(&buffer.data))
              {
                return MakeRawImageData(vector->bytes.data() + bufferView.byteOffset,
                                        bufferView.byteLength,
                                        view->mimeType,
                                        image.name);
              }
            }

            return RawImageData{};
          }();

          if (rawImage.isKtx)
          {
            ktxTexture2* ktx{};
            if (auto result =
                  ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(rawImage.encodedPixelData.get()),
                                               rawImage.encodedPixelSize,
                                               KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                               &ktx);
                result != KTX_SUCCESS)
            {
              FWOG_UNREACHABLE;
            }

            rawImage.width = ktx->baseWidth;
            rawImage.height = ktx->baseHeight;
            rawImage.components = ktxTexture2_GetNumComponents(ktx);
            rawImage.ktx.reset(ktx);
          }
          else
          {
            int x, y, comp;
            auto* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(rawImage.encodedPixelData.get()),
                                                 rawImage.encodedPixelSize,
                                                 &x,
                                                 &y,
                                                 &comp,
                                                 4);

            FWOG_ASSERT(pixels != nullptr);

            rawImage.width = x;
            rawImage.height = y;
            // rawImage.components = comp;
            rawImage.components = 4; // If forced 4 components
            rawImage.data.reset(pixels);
          }

          return rawImage;
        });

      // Upload image data to GPU
      auto loadedImages = std::vector<Fwog::Texture>();
      loadedImages.reserve(rawImageData.size());

      for (const auto& image : rawImageData)
      {
        Fwog::Extent2D dims = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)};

        // Upload KTX2 compressed image
        if (image.isKtx)
        {
          auto* ktx = image.ktx.get();

          auto format = Fwog::Format::BC7_RGBA_UNORM;

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

          loadedImages.emplace_back(std::move(textureData));
        }
        else // Upload raw image data and generate mipmap
        {
          FWOG_ASSERT(image.components == 4);
          FWOG_ASSERT(image.pixel_type == GL_UNSIGNED_BYTE);
          FWOG_ASSERT(image.bits == 8);

          auto textureData = Fwog::CreateTexture2DMip(dims,
                                                      Fwog::Format::R8G8B8A8_UNORM,
                                                      uint32_t(1 + floor(log2(glm::max(dims.width, dims.height)))),
                                                      image.name);

          auto updateInfo = Fwog::TextureUpdateInfo{
            .level = 0,
            .offset = {},
            .extent = {dims.width, dims.height, 1},
            .format = Fwog::UploadFormat::RGBA,
            .type = Fwog::UploadType::UBYTE,
            .pixels = image.data.get(),
          };
          textureData.UpdateImage(updateInfo);
          textureData.GenMipmaps();

          loadedImages.emplace_back(std::move(textureData));
        }
      }

      return loadedImages;
    }

    glm::mat4 NodeToMat4(const fastgltf::Node& node)
    {
      glm::mat4 transform{1};

      if (auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform))
      {
        auto rotation = glm::quat{trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]};
        auto scale = glm::vec3{trs->scale[0], trs->scale[1], trs->scale[2]};
        auto translation = glm::vec3{trs->translation[0], trs->translation[1], trs->translation[2]};

        glm::mat4 rotationMat = glm::mat4_cast(rotation);

        // T * R * S
        transform = glm::scale(glm::translate(translation) * rotationMat, scale);
      }
      else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform))
      {
        const auto& m = *mat;
        // clang-format off
        transform = { 
          m[0], m[1], m[2], m[3], 
          m[4], m[5], m[6], m[7], 
          m[8], m[9], m[10], m[11], 
          m[12], m[13], m[14], m[15], };
        // clang-format on
      }
      // else node has identity transform

      return transform;
    }
  } // namespace

  std::vector<Vertex> ConvertVertexBufferFormat(const fastgltf::Asset& model, const fastgltf::Primitive& primitive)
  {
    std::vector<glm::vec3> positions;
    auto& positionAccessor = model.accessors[primitive.findAttribute("POSITION")->second];
    positions.resize(positionAccessor.count);
    fastgltf::iterateAccessorWithIndex<glm::vec3>(model,
                                                  positionAccessor,
                                                  [&](glm::vec3 position, std::size_t idx) { positions[idx] = position; });

    std::vector<glm::vec3> normals;
    auto& normalAccessor = model.accessors[primitive.findAttribute("NORMAL")->second];
    normals.resize(normalAccessor.count);
    fastgltf::iterateAccessorWithIndex<glm::vec3>(model,
                                                  normalAccessor,
                                                  [&](glm::vec3 normal, std::size_t idx) { normals[idx] = normal; });

    std::vector<glm::vec2> texcoords;

    // Textureless meshes will use factors instead of textures
    if (primitive.findAttribute("TEXCOORD_0") != primitive.attributes.end())
    {
      auto& texcoordAccessor = model.accessors[primitive.findAttribute("TEXCOORD_0")->second];
      texcoords.resize(texcoordAccessor.count);
      fastgltf::iterateAccessorWithIndex<glm::vec2>(model,
                                                    texcoordAccessor,
                                                    [&](glm::vec2 texcoord, std::size_t idx)
                                                    { texcoords[idx] = texcoord; });
    }
    else
    {
      // If no texcoord attribute, fill with empty texcoords to keep everything consistent and happy
      texcoords.resize(positions.size(), {});
    }

    FWOG_ASSERT(positions.size() == normals.size() && positions.size() == texcoords.size());

    std::vector<Vertex> vertices;
    vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++)
    {
      vertices[i] = {positions[i], glm::packSnorm2x16(float32x3_to_oct(normals[i])), texcoords[i]};
    }

    return vertices;
  }

  std::vector<index_t> ConvertIndexBufferFormat(const fastgltf::Asset& model, const fastgltf::Primitive& primitive)
  {
    auto indices = std::vector<index_t>();
    auto& accessor = model.accessors[primitive.indicesAccessor.value()];
    indices.resize(accessor.count);
    fastgltf::iterateAccessorWithIndex<index_t>(model, accessor, [&](index_t index, size_t idx) { indices[idx] = index; });
    return indices;
  }

  std::vector<Material> LoadMaterials(const fastgltf::Asset& model, std::span<Fwog::Texture> images)
  {
    auto LoadSampler = [](const fastgltf::Sampler& sampler)
    {
      Fwog::SamplerState samplerState{};

      samplerState.addressModeU = ConvertGlAddressMode((GLint)sampler.wrapS);
      samplerState.addressModeV = ConvertGlAddressMode((GLint)sampler.wrapT);
      if (sampler.minFilter.has_value())
      {
        samplerState.minFilter = ConvertGlFilterMode((GLint)sampler.minFilter.value());
        samplerState.mipmapFilter = GetGlMipmapFilter((GLint)sampler.minFilter.value());

        if (samplerState.minFilter != Fwog::Filter::NONE)
        {
          samplerState.anisotropy = Fwog::SampleCount::SAMPLES_16;
        }
      }
      if (sampler.magFilter.has_value())
      {
        samplerState.magFilter = ConvertGlFilterMode((GLint)sampler.magFilter.value());
      }

      return samplerState;
    };

    std::vector<Material> materials;

    for (const auto& loaderMaterial : model.materials)
    {
      FWOG_ASSERT(loaderMaterial.pbrData.has_value());

      glm::vec4 baseColorFactor{};
      for (int i = 0; i < 4; i++)
      {
        loaderMaterial.pbrData->baseColorFactor;
        baseColorFactor[i] = static_cast<float>(loaderMaterial.pbrData->baseColorFactor[i]);
      }

      Material material;

      if (loaderMaterial.pbrData->baseColorTexture.has_value())
      {
        auto baseColorTextureIndex = loaderMaterial.pbrData->baseColorTexture->textureIndex;
        const auto& baseColorTexture = model.textures[baseColorTextureIndex];
        auto& image = images[baseColorTexture.imageIndex.value()];
        material.gpuMaterial.flags |= MaterialFlagBit::HAS_BASE_COLOR_TEXTURE;
        material.albedoTextureSampler = {image.CreateFormatView(FormatToSrgb(image.GetCreateInfo().format)),
                                         LoadSampler(model.samplers[baseColorTexture.samplerIndex.value()])};
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
    glm::vec3 min{1e20f};
    glm::vec3 max{-1e20f};
    for (const auto& vertex : vertices)
    {
      min = glm::min(min, vertex.position);
      max = glm::max(max, vertex.position);
    }

    return Box3D{
      .offset = (min + max) / 2.0f,
      .halfExtent = (max - min) / 2.0f,
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
  };

  std::optional<LoadModelResult> LoadModelFromFileBase(std::filesystem::path path,
                                                       glm::mat4 rootTransform,
                                                       bool binary,
                                                       uint32_t baseMaterialIndex)
  {
    using fastgltf::Extensions;
    auto parser = fastgltf::Parser(Extensions::KHR_texture_basisu | Extensions::KHR_mesh_quantization |
                                   Extensions::EXT_meshopt_compression | Extensions::KHR_lights_punctual);

    auto data = fastgltf::GltfDataBuffer();
    data.loadFromFile(path);

    Timer timer;

    std::unique_ptr<fastgltf::glTF> gltf{};
    constexpr auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
                             fastgltf::Options::LoadGLBBuffers;
    if (binary)
    {
      gltf = parser.loadBinaryGLTF(&data, path.parent_path(), options);
    }
    else
    {
      gltf = parser.loadGLTF(&data, path.parent_path(), options);
    }

    if (auto err = parser.getError(); err != fastgltf::Error::None)
    {
      std::cout << "glTF error: " << static_cast<uint64_t>(err) << '\n';
      return std::nullopt;
    }

    if (auto err = gltf->parse(fastgltf::Category::OnlyRenderable); err != fastgltf::Error::None)
    {
      std::cout << "glTF error: " << static_cast<uint64_t>(err) << '\n';
      return std::nullopt;
    }

    auto assetPtr = gltf->getParsedAsset();
    auto& asset = *assetPtr;

    // Let's not deal with glTFs containing multiple scenes right now
    FWOG_ASSERT(asset.scenes.size() == 1);

    // Load images and boofers
    auto images = LoadImages(asset);

    auto ms = timer.Elapsed_us() / 1000;
    std::cout << "Loading took " << ms << " ms\n";

    LoadModelResult scene;

    auto materials = LoadMaterials(asset, images);
    std::ranges::move(materials, std::back_inserter(scene.materials));

    // <node*, global transform>
    std::stack<std::pair<const fastgltf::Node*, glm::mat4>> nodeStack;

    for (auto nodeIndex : asset.scenes[0].nodeIndices)
    {
      nodeStack.emplace(&asset.nodes[nodeIndex], rootTransform);
    }

    while (!nodeStack.empty())
    {
      decltype(nodeStack)::value_type top = nodeStack.top();
      const auto& [node, parentGlobalTransform] = top;
      nodeStack.pop();

      // std::cout << "Node: " << node->name << '\n';

      glm::mat4 localTransform = NodeToMat4(*node);
      glm::mat4 globalTransform = parentGlobalTransform * localTransform;

      for (auto childNodeIndex : node->children)
      {
        nodeStack.emplace(&asset.nodes[childNodeIndex], globalTransform);
      }

      if (node->meshIndex.has_value())
      {
        // TODO: get a reference to the mesh instead of loading it from scratch
        for (const fastgltf::Mesh& mesh = asset.meshes[node->meshIndex.value()]; const auto& primitive : mesh.primitives)
        {
          auto vertices = ConvertVertexBufferFormat(asset, primitive);
          auto indices = ConvertIndexBufferFormat(asset, primitive);

          scene.meshes.emplace_back(CpuMesh{
            std::move(vertices),
            std::move(indices),
            baseMaterialIndex + std::max(uint32_t(primitive.materialIndex.value()), uint32_t(0)),
            globalTransform,
          });
        }
      }
    }

    std::cout << "Loaded glTF: " << path << '\n';

    return scene;
  }

  bool LoadModelFromFile(Scene& scene, std::string_view fileName, glm::mat4 rootTransform, bool binary)
  {
    const auto baseMaterialIndex = static_cast<uint32_t>(scene.materials.size());

    auto loadedScene = LoadModelFromFileBase(fileName, rootTransform, binary, baseMaterialIndex);

    if (!loadedScene)
      return false;

    scene.meshes.reserve(scene.meshes.size() + loadedScene->meshes.size());
    for (auto& mesh : loadedScene->meshes)
    {
      scene.meshes.emplace_back(Mesh{
        .vertexBuffer = Fwog::Buffer(std::span(mesh.vertices)),
        .indexBuffer = Fwog::Buffer(std::span(mesh.indices)),
        .materialIdx = mesh.materialIdx,
        .transform = mesh.transform,
      });
    }

    std::ranges::move(loadedScene->materials, std::back_inserter(scene.materials));

    return true;
  }

  bool LoadModelFromFileBindless(SceneBindless& scene, std::string_view fileName, glm::mat4 rootTransform, bool binary)
  {
    FWOG_ASSERT(scene.textures.size() == scene.samplers.size());
    const auto baseMaterialIndex = static_cast<uint32_t>(scene.materials.size());

    auto loadedScene = LoadModelFromFileBase(fileName, rootTransform, binary, baseMaterialIndex);

    if (!loadedScene)
      return false;

    scene.meshes.reserve(scene.meshes.size() + loadedScene->meshes.size());
    for (auto& mesh : loadedScene->meshes)
    {
      scene.meshes.emplace_back(MeshBindless{
        .startVertex = static_cast<int32_t>(scene.vertices.size()),
        .startIndex = static_cast<uint32_t>(scene.indices.size()),
        .indexCount = static_cast<uint32_t>(mesh.indices.size()),
        .materialIdx = mesh.materialIdx,
        .transform = mesh.transform,
        .boundingBox = GetBoundingBox(mesh.vertices),
      });

      std::vector<Vertex> tempVertices = std::move(mesh.vertices);
      scene.vertices.insert(scene.vertices.end(), tempVertices.begin(), tempVertices.end());

      std::vector<index_t> tempIndices = std::move(mesh.indices);
      scene.indices.insert(scene.indices.end(), tempIndices.begin(), tempIndices.end());
    }

    scene.materials.reserve(scene.materials.size() + loadedScene->materials.size());
    for (auto& material : loadedScene->materials)
    {
      GpuMaterialBindless bindlessMaterial{
        .flags = material.gpuMaterial.flags,
        .alphaCutoff = material.gpuMaterial.alphaCutoff,
        .baseColorTextureHandle = 0,
        .baseColorFactor = material.gpuMaterial.baseColorFactor,
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
} // namespace Utility
