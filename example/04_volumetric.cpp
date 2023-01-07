/* volumetric.cpp
 *
 * Volumetric fog viewer.
 *
 * Takes the same command line arguments as the gltf_viewer example.
 */

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "common/Application.h"

#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/DebugMarker.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "common/SceneLoader.h"

#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>

/* 04_volumetric
 *
 * A renderer with volumetric fog effects. The volumetric effect is computed in a volume texture, 
 * independent of the screen resolution, then applied in a separate pass. The technique is largely
 * based on a presentation by Bart Wronski titled "Volumetric Fog".
 *
 * The app has the same options as 03_gltf_viewer.
 *
 * Options
 * Filename (string) : name of the glTF file you wish to view.
 * Scale (real)      : uniform scale factor in case the model is tiny or huge. Default: 1.0
 * Binary (int)      : whether the input file is binary glTF. Default: false
 *
 * If no options are specified, the default scene will be loaded.
 *
 */

// not needed because SceneLoader implements stb_include
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

////////////////////////////////////// Types

struct ObjectUniforms
{
  glm::mat4 model;
};

struct GlobalUniforms
{
  glm::mat4 viewProj;
  glm::mat4 invViewProj;
  glm::vec4 cameraPos;
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
};

struct Light
{
  glm::vec4 position;
  glm::vec3 intensity;
  float invRadius;
};

struct EsmBlurUniforms
{
  glm::ivec2 direction;
  glm::ivec2 targetDim;
};

glm::mat4 InfReverseZPerspectiveRH(float fovY_radians, float aspectWbyH, float zNear)
{
  float f = 1.0f / tan(fovY_radians / 2.0f);
  return glm::mat4(f / aspectWbyH, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, zNear, 0.0f);
}

struct
{
  Fwog::Extent3D shadowmapResolution = {2048, 2048};

  float viewNearPlane = 0.3f;

  float esmExponent = 40.0f;
  size_t esmBlurPasses = 1;
  Fwog::Extent3D esmResolution = {512, 512};

  float volumeNearPlane = viewNearPlane;
  float volumeFarPlane = 60.0f;
  Fwog::Extent3D volumeExtent = {160, 90, 256};
  bool volumeUseScatteringTexture = true;
  float volumeAnisotropyG = 0.2f;
  float volumeNoiseOffsetScale = 0.0f;
  bool frog = false;
  float volumetricGroundFogDensity = .15f;

  float lightFarPlane = 50.0f;
  float lightProjWidth = 24.0f;
  float lightDistance = 25.0f;
} config;

static constexpr std::array<Fwog::VertexInputBindingDescription, 3> sceneInputBindingDescs{
  Fwog::VertexInputBindingDescription{
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Utility::Vertex, position),
  },
  Fwog::VertexInputBindingDescription{
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R16G16_SNORM,
    .offset = offsetof(Utility::Vertex, normal),
  },
  Fwog::VertexInputBindingDescription{
    .location = 2,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = offsetof(Utility::Vertex, texcoord),
  },
};

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/SceneDeferredSimple.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::GREATER},
  });
}

Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .vertexInputState = {sceneInputBindingDescs},
    .rasterizationState =
      {
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 3.0f,
        .depthBiasSlopeFactor = 5.0f,
      },
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true},
  });
}

Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/ShadeDeferredSimple.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
    .depthState = {.depthTestEnable = false, .depthWriteEnable = false},
  });
}

Fwog::GraphicsPipeline CreateDebugTexturePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/Texture.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
    .depthState = {.depthTestEnable = false, .depthWriteEnable = false},
  });
}

Fwog::ComputePipeline CreateCopyToEsmPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/volumetric/Depth2exponential.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

Fwog::ComputePipeline CreateGaussianBlurPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/volumetric/GaussianBlur.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

Fwog::ComputePipeline CreatePostprocessingPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/volumetric/TonemapAndDither.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

class VolumetricTechnique
{
public:
  void Init()
  {
    char error[256] = {};
    char* accumulateDensity =
      stb_include_string(Application::LoadFile("shaders/volumetric/CellLightingAndDensity.comp.glsl").data(),
                         nullptr,
                         "shaders/volumetric",
                         "CellLightingAndDensity",
                         error);

    char* marchVolume = stb_include_string(Application::LoadFile("shaders/volumetric/MarchVolume.comp.glsl").data(),
                                           nullptr,
                                           "shaders/volumetric",
                                           "marchVolume",
                                           error);

    char* applyDeferred =
      stb_include_string(Application::LoadFile("shaders/volumetric/ApplyVolumetricsDeferred.comp.glsl").data(),
                         nullptr,
                         "shaders/volumetric",
                         "applyDeferred",
                         error);

    std::string infoLog;
    auto accumulateShader = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, accumulateDensity);
    auto marchShader = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, marchVolume);
    auto applyShader = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, applyDeferred);

    free(applyDeferred);
    free(marchVolume);
    free(accumulateDensity);

    accumulateDensityPipeline = Fwog::ComputePipeline({.shader = &accumulateShader});
    marchVolumePipeline = Fwog::ComputePipeline({.shader = &marchShader});
    applyDeferredPipeline = Fwog::ComputePipeline({.shader = &applyShader});

    // Load the normalized MiePlot generated scattering data.
    // This texture is used if a flag is set in marchVolume.comp.glsl.
    std::ifstream file{"textures/fog_mie_data.txt"};

    std::vector<glm::vec3> data;
    data.reserve(500);

    while (file.peek() != EOF)
    {
      std::string fs0, fs1, fs2;
      std::getline(file, fs0);
      std::getline(file, fs1);
      std::getline(file, fs2);

      float blue = std::stof(fs0);
      float green = std::stof(fs1);
      float red = std::stof(fs2);

      data.push_back({red, green, blue});
    }

    scatteringTexture = Fwog::Texture(Fwog::TextureCreateInfo{
      .imageType = Fwog::ImageType::TEX_1D,
                                                              .format = Fwog::Format::R16G16B16_FLOAT,
                                                              .extent = {static_cast<uint32_t>(data.size())},
                                                              .mipLevels = 1,
                                                              .arrayLayers = 1,
      .sampleCount = Fwog::SampleCount::SAMPLES_1,
    });

    scatteringTexture->SubImage({
      .dimension = Fwog::UploadDimension::ONE,
                                 .size = {static_cast<uint32_t>(data.size())},
                                 .format = Fwog::UploadFormat::RGB,
                                 .type = Fwog::UploadType::FLOAT,
      .pixels = data.data(),
    });
  }

  void UpdateUniforms(const View& view,
                      const glm::mat4& projCamera,
                      const glm::mat4& sunViewProj,
                      glm::vec3 sunDir,
                      float fovy,
                      float aspectRatio,
                      float volumeNearPlane,
                      float volumeFarPlane,
                      float time,
                      bool useScatteringTexture,
                      float isotropyG,
                      float noiseOffsetScale,
                      bool frog,
                      float groundFogDensity,
                      glm::vec3 sunColor)
  {
    glm::mat4 projVolume = glm::perspectiveZO(fovy, aspectRatio, volumeNearPlane, volumeFarPlane);
    glm::mat4 viewMat = view.GetViewMatrix();
    glm::mat4 viewProjVolume = projVolume * viewMat;

    struct
    {
      glm::vec3 viewPos;
      float time;
      glm::mat4 invViewProjScene;
      glm::mat4 viewProjVolume;
      glm::mat4 invViewProjVolume;
      glm::mat4 sunViewProj;
      glm::vec3 sunDir;
      float volumeNearPlane;
      float volumeFarPlane;
      uint32_t useScatteringTexture;
      float isotropyG;
      float noiseOffsetScale;
      uint32_t frog;
      float groundFogDensity;
      uint32_t _padding00;
      uint32_t _padding01;
      glm::vec3 sunColor;
    } uniforms;

    uniforms = {.viewPos = view.position,
                .time = time,
                .invViewProjScene = glm::inverse(projCamera * viewMat),
                .viewProjVolume = viewProjVolume,
                .invViewProjVolume = glm::inverse(viewProjVolume),
                .sunViewProj = sunViewProj,
                .sunDir = sunDir,
                .volumeNearPlane = volumeNearPlane,
                .volumeFarPlane = volumeFarPlane,
                .useScatteringTexture = useScatteringTexture,
                .isotropyG = isotropyG,
                .noiseOffsetScale = noiseOffsetScale,
                .frog = frog,
                .groundFogDensity = groundFogDensity,
                .sunColor = sunColor};

    if (!uniformBuffer)
    {
      uniformBuffer = Fwog::Buffer(sizeof(uniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
    }
    uniformBuffer->SubData(uniforms, 0);
  }

  void AccumulateDensity(const Fwog::Texture& densityVolume,
                         const Fwog::Texture& shadowDepth,
                         const Fwog::Buffer& esmUniformBuffer,
                         const Fwog::Buffer& lightBuffer)
  {
    assert(densityVolume.CreateInfo().imageType == Fwog::ImageType::TEX_3D);

    auto sampler = Fwog::Sampler({.minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

    Fwog::BeginCompute();
    Fwog::Cmd::BindComputePipeline(*accumulateDensityPipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindUniformBuffer(1, esmUniformBuffer, 0, esmUniformBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(0, lightBuffer, 0, lightBuffer.Size());
    Fwog::Cmd::BindSampledImage(0, shadowDepth, sampler);
    Fwog::Cmd::BindSampledImage(1, *scatteringTexture, sampler);
    Fwog::Cmd::BindImage(0, densityVolume, 0);
    Fwog::Extent3D numGroups = (densityVolume.Extent() + 7) / 8;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, numGroups.depth);
    Fwog::EndCompute();
  }

  void MarchVolume(const Fwog::Texture& sourceVolume, const Fwog::Texture& targetVolume)
  {
    assert(sourceVolume.CreateInfo().imageType == Fwog::ImageType::TEX_3D);
    assert(targetVolume.CreateInfo().imageType == Fwog::ImageType::TEX_3D);

    auto sampler = Fwog::Sampler({.minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

    Fwog::BeginCompute();
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::BindComputePipeline(*marchVolumePipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindSampledImage(0, sourceVolume, sampler);
    Fwog::Cmd::BindImage(0, targetVolume, 0);
    Fwog::Extent3D numGroups = (targetVolume.Extent() + 15) / 16;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, 1);
    Fwog::EndCompute();
  }

  void ApplyDeferred(const Fwog::Texture& gbufferColor,
                     const Fwog::Texture& gbufferDepth,
                     const Fwog::Texture& targetColor,
                     const Fwog::Texture& sourceVolume,
                     const Fwog::Texture& noise)
  {
    assert(sourceVolume.CreateInfo().imageType == Fwog::ImageType::TEX_3D);
    assert(targetColor.Extent() == gbufferColor.Extent() && targetColor.Extent() == gbufferDepth.Extent());

    auto sampler = Fwog::Sampler({.minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

    Fwog::BeginCompute();
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::BindComputePipeline(*applyDeferredPipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindSampledImage(0, gbufferColor, sampler);
    Fwog::Cmd::BindSampledImage(1, gbufferDepth, sampler);
    Fwog::Cmd::BindSampledImage(2, sourceVolume, sampler);
    Fwog::Cmd::BindSampledImage(3, noise, sampler);
    Fwog::Cmd::BindImage(0, targetColor, 0);
    Fwog::Extent2D numGroups = (targetColor.Extent() + 15) / 16;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, 1);
    Fwog::EndCompute();
  }

private:
  std::optional<Fwog::ComputePipeline> accumulateDensityPipeline;
  std::optional<Fwog::ComputePipeline> marchVolumePipeline;
  std::optional<Fwog::ComputePipeline> applyDeferredPipeline;
  std::optional<Fwog::Buffer> uniformBuffer;
  std::optional<Fwog::Texture> scatteringTexture;
};

class VolumetricApplication final : public Application
{
public:
  VolumetricApplication(const Application::CreateInfo& createInfo,
                        std::optional<std::string_view> filename,
                        float scale,
                        bool binary);

private:
  void OnWindowResize(uint32_t newWidth, uint32_t newHeight) override;
  void OnUpdate(double dt) override;
  void OnRender(double dt) override;
  void OnGui(double dt) override;

  double volumetricTime = 0;

  float sunPosition = -1.127f;
  float sunStrength = 3;
  glm::vec3 sunColor = {1, 1, 1};

  struct Frame
  {
    std::optional<Fwog::Texture> gAlbedo;
    std::optional<Fwog::Texture> gNormal;
    std::optional<Fwog::Texture> gDepth;
    std::optional<Fwog::Texture> shadingTexHdr;
    std::optional<Fwog::Texture> shadingTexLdr;
  };
  Frame frame{};

  VolumetricTechnique volumetric{};
  Fwog::Texture densityVolume;
  Fwog::Texture scatteringVolume;

  Fwog::Texture shadowDepth;

  Fwog::Texture esmTex;
  Fwog::Texture esmTexPingPong;
  Fwog::TypedBuffer<float> esmUniformBuffer;
  Fwog::TypedBuffer<EsmBlurUniforms> esmBlurUniformBuffer;

  ShadingUniforms shadingUniforms;
  std::optional<Fwog::Texture> noiseTexture;

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;
  Fwog::TypedBuffer<ShadingUniforms> shadingUniformsBuffer;
  Fwog::TypedBuffer<Utility::GpuMaterial> materialUniformsBuffer;

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline shadowPipeline;
  Fwog::GraphicsPipeline shadingPipeline;
  Fwog::GraphicsPipeline debugTexturePipeline;
  Fwog::ComputePipeline copyToEsmPipeline;
  Fwog::ComputePipeline gaussianBlurPipeline;
  Fwog::ComputePipeline postprocessingPipeline;

  Utility::Scene scene;
  std::optional<Fwog::TypedBuffer<Light>> lightBuffer;
  std::optional<Fwog::TypedBuffer<ObjectUniforms>> meshUniformBuffer;
};

VolumetricApplication::VolumetricApplication(const Application::CreateInfo& createInfo,
                                             std::optional<std::string_view> filename,
                                             float scale,
                                             bool binary)
  : Application(createInfo),
    densityVolume({
      .imageType = Fwog::ImageType::TEX_3D,
      .format = Fwog::Format::R16G16B16A16_FLOAT,
      .extent = config.volumeExtent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .sampleCount = Fwog::SampleCount::SAMPLES_1,
    }),
    scatteringVolume({
      .imageType = Fwog::ImageType::TEX_3D,
      .format = Fwog::Format::R16G16B16A16_FLOAT,
      .extent = config.volumeExtent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .sampleCount = Fwog::SampleCount::SAMPLES_1,
    }),
    shadowDepth(Fwog::CreateTexture2D(config.shadowmapResolution, Fwog::Format::D16_UNORM)),
    esmTex(Fwog::CreateTexture2D(config.esmResolution, Fwog::Format::R32_FLOAT)),
    esmTexPingPong(Fwog::CreateTexture2D(config.esmResolution, Fwog::Format::R32_FLOAT)),
    esmUniformBuffer(config.esmExponent, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    esmBlurUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE, Fwog::BufferMapFlag::MAP_WRITE),
    shadingUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    materialUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    scenePipeline(CreateScenePipeline()),
    shadowPipeline(CreateShadowPipeline()),
    shadingPipeline(CreateShadingPipeline()),
    debugTexturePipeline(CreateDebugTexturePipeline()),
    copyToEsmPipeline(CreateCopyToEsmPipeline()),
    gaussianBlurPipeline(CreateGaussianBlurPipeline()),
    postprocessingPipeline(CreatePostprocessingPipeline())
{
  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  cursorIsActive = true;
  mainCamera.position = {0, 1.5, 2};
  mainCamera.yaw = -glm::half_pi<float>();

  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  if (!filename)
  {
    Utility::LoadModelFromFile(scene, "models/simple_scene.glb", glm::mat4{.5}, true);
  }
  else
  {
    Utility::LoadModelFromFile(scene, *filename, glm::scale(glm::vec3{scale}), binary);
  }

  std::vector<ObjectUniforms> meshUniforms;
  for (size_t i = 0; i < scene.meshes.size(); i++)
  {
    meshUniforms.push_back({scene.meshes[i].transform});
  }

  std::vector<Light> lights;
  lights.push_back(Light{.position = {-3, 1, -1, 0}, .intensity = {.2f, .8f, 1.0f}, .invRadius = 1.0f / 4.0f});
  lights.push_back(Light{.position = {3, 2, 0, 0}, .intensity = {.7f, .8f, 0.1f}, .invRadius = 1.0f / 2.0f});
  lights.push_back(Light{.position = {3, 3, 2, 0}, .intensity = {1.2f, .8f, .1f}, .invRadius = 1.0f / 6.0f});
  lights.push_back(Light{.position = {.9, 5.5, -1.65, 0}, .intensity = {5.2f, 4.8f, 12.5f}, .invRadius = 1.0f / 9.0f});

  meshUniformBuffer = Fwog::TypedBuffer<ObjectUniforms>(std::span(meshUniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  lightBuffer = Fwog::TypedBuffer<Light>(std::span(lights), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  int x = 0;
  int y = 0;
  auto noise = stbi_load("textures/bluenoise32.png", &x, &y, nullptr, 4);
  assert(noise);
  noiseTexture = Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
  noiseTexture->SubImage({
    .dimension = Fwog::UploadDimension::TWO,
    .size = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
    .format = Fwog::UploadFormat::RGBA,
    .type = Fwog::UploadType::UBYTE,
    .pixels = noise,
  });
  stbi_image_free(noise);

  volumetric.Init();

  OnWindowResize(windowWidth, windowHeight);
}

void VolumetricApplication::OnWindowResize(uint32_t newWidth, uint32_t newHeight)
{
  frame.gAlbedo = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_UNORM);
  frame.gNormal = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepth = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_FLOAT);
  frame.shadingTexHdr = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16A16_FLOAT);
  frame.shadingTexLdr = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_UNORM);
}

void VolumetricApplication::OnUpdate([[maybe_unused]] double dt) {}

void VolumetricApplication::OnRender([[maybe_unused]] double dt)
{
  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  ss.compareEnable = true;
  ss.compareOp = Fwog::CompareOp::LESS_OR_EQUAL;
  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  auto shadowSampler = Fwog::Sampler(ss);

  shadingUniforms = ShadingUniforms{
    .sunDir = glm::normalize(glm::rotate(sunPosition, glm::vec3{1, 0, 0}) * glm::vec4{-.1, -.3, -.6, 0}),
    .sunStrength = glm::vec4{sunStrength * sunColor, 0},
  };
  shadingUniformsBuffer.SubDataTyped(shadingUniforms);

  // update global uniforms
  const auto fovy = glm::radians(70.f);
  const auto aspectRatio = windowWidth / (float)windowHeight;
  auto proj = InfReverseZPerspectiveRH(fovy, aspectRatio, config.viewNearPlane);

  GlobalUniforms mainCameraUniforms{};
  mainCameraUniforms.viewProj = proj * mainCamera.GetViewMatrix();
  mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
  mainCameraUniforms.cameraPos = glm::vec4(mainCamera.position, 0.0);
  globalUniformsBuffer.SubDataTyped(mainCameraUniforms);

  glm::vec3 eye = glm::vec3{-shadingUniforms.sunDir * config.lightDistance};
  shadingUniforms.sunViewProj = glm::orthoZO(-config.lightProjWidth,
                                             config.lightProjWidth,
                                             -config.lightProjWidth,
                                             config.lightProjWidth,
                                             0.f,
                                             config.lightFarPlane) *
                                glm::lookAt(eye, glm::vec3(0), glm::vec3{0, 1, 0});
  shadingUniformsBuffer.SubData(shadingUniforms, 0);

  // geometry buffer pass
  {
    Fwog::RenderAttachment gAlbedoAttachment{
      .texture = &frame.gAlbedo.value(),
      .clearValue = Fwog::ClearColorValue{.1f, .3f, .5f, 0.0f},
      .clearOnLoad = true,
    };
    Fwog::RenderAttachment gNormalAttachment{
      .texture = &frame.gNormal.value(),
      .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
      .clearOnLoad = false,
    };
    Fwog::RenderAttachment gDepthAttachment{
      .texture = &frame.gDepth.value(),
      .clearValue = Fwog::ClearDepthStencilValue{.depth = 0.0f},
      .clearOnLoad = true,
    };
    Fwog::RenderAttachment cgAttachments[] = {gAlbedoAttachment, gNormalAttachment};
    Fwog::RenderInfo gbufferRenderInfo{
      .colorAttachments = cgAttachments,
      .depthAttachment = &gDepthAttachment,
      .stencilAttachment = nullptr,
    };
    Fwog::BeginRendering(gbufferRenderInfo);
    Fwog::ScopedDebugMarker marker("Geometry");
    Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer, 0, materialUniformsBuffer.Size());

    Fwog::Cmd::BindStorageBuffer(1, meshUniformBuffer.value(), 0, meshUniformBuffer->Size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
    {
      const auto& mesh = scene.meshes[i];
      const auto& material = scene.materials[mesh.materialIdx];
      materialUniformsBuffer.SubData(material.gpuMaterial, 0);
      if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
      {
        const auto& textureSampler = scene.textureSamplers[material.baseColorTextureIdx];
        Fwog::Cmd::BindSampledImage(0, textureSampler.texture, textureSampler.sampler);
      }
      Fwog::Cmd::BindVertexBuffer(0, mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
      Fwog::Cmd::BindIndexBuffer(mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer.Size()) / sizeof(uint32_t), 1, 0, 0, i);
    }
    Fwog::EndRendering();
  }

  globalUniformsBuffer.SubData(shadingUniforms.sunViewProj, offsetof(GlobalUniforms, viewProj));

  // shadow map scene pass
  {
    Fwog::RenderAttachment depthAttachment{
      .texture = &shadowDepth,
      .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
      .clearOnLoad = true,
    };

    Fwog::RenderInfo shadowRenderInfo{.depthAttachment = &depthAttachment, .stencilAttachment = nullptr};
    Fwog::BeginRendering(shadowRenderInfo);
    Fwog::ScopedDebugMarker marker("Shadow Scene");
    Fwog::Cmd::BindGraphicsPipeline(shadowPipeline);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(1, meshUniformBuffer.value(), 0, meshUniformBuffer->Size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
    {
      const auto& mesh = scene.meshes[i];
      Fwog::Cmd::BindVertexBuffer(0, mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
      Fwog::Cmd::BindIndexBuffer(mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer.Size()) / sizeof(uint32_t), 1, 0, 0, i);
    }
    Fwog::EndRendering();
  }

  // copy to esm and blur
  {
    Fwog::BeginCompute();
    {
      {
        Fwog::ScopedDebugMarker marker("Copy to ESM");
        esmUniformBuffer.SubData(config.esmExponent, 0);

        auto nearestMirrorSampler = Fwog::Sampler({.minFilter = Fwog::Filter::NEAREST,
                                                   .magFilter = Fwog::Filter::NEAREST,
                                                   .addressModeU = Fwog::AddressMode::MIRRORED_REPEAT,
                                                   .addressModeV = Fwog::AddressMode::MIRRORED_REPEAT});
        Fwog::Cmd::BindComputePipeline(copyToEsmPipeline);
        Fwog::Cmd::BindSampledImage(0, shadowDepth, nearestMirrorSampler);
        Fwog::Cmd::BindImage(0, esmTex, 0);
        Fwog::Cmd::BindUniformBuffer(0, esmUniformBuffer, 0, esmUniformBuffer.Size());
        auto dispatchDim = (esmTex.Extent() + 7) / 8;
        Fwog::Cmd::Dispatch(dispatchDim.width, dispatchDim.height, 1);

        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
      }

      // blur
      {
        Fwog::ScopedDebugMarker marker("Blur ESM");
        Fwog::Cmd::BindComputePipeline(gaussianBlurPipeline);

        auto linearSampler = Fwog::Sampler({.minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

        EsmBlurUniforms esmBlurUniforms{};

        const auto esmExtent1 = esmTex.Extent();
        const auto esmExtent2 = esmTexPingPong.Extent();

        const auto dispatchSize1 = (esmExtent2 + 7) / 8;
        const auto dispatchSize2 = (esmExtent1 + 7) / 8;

        Fwog::Cmd::BindUniformBuffer(0, esmBlurUniformBuffer, 0, esmBlurUniformBuffer.Size());

        for (size_t i = 0; i < config.esmBlurPasses; i++)
        {
          esmBlurUniforms.direction = {0, 1};
          esmBlurUniforms.targetDim = {esmExtent2.width, esmExtent2.height};
          esmBlurUniformBuffer.SubData(esmBlurUniforms, 0);
          Fwog::Cmd::BindSampledImage(0, esmTex, linearSampler);
          Fwog::Cmd::BindImage(0, esmTexPingPong, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(dispatchSize1.width, dispatchSize1.height, 1);

          esmBlurUniforms.direction = {1, 0};
          esmBlurUniforms.targetDim = {esmExtent1.width, esmExtent1.height};
          esmBlurUniformBuffer.SubData(esmBlurUniforms, 0);
          Fwog::Cmd::BindSampledImage(0, esmTexPingPong, linearSampler);
          Fwog::Cmd::BindImage(0, esmTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(dispatchSize2.width, dispatchSize2.height, 1);
        }
      }
    }
    Fwog::EndCompute();
  }

  globalUniformsBuffer.SubData(mainCameraUniforms, 0);

  // shading pass (full screen tri)
  {
    Fwog::RenderAttachment shadingAttachment{.texture = &frame.shadingTexHdr.value(), .clearOnLoad = false};

    Fwog::RenderInfo shadingRenderingInfo{.colorAttachments = {&shadingAttachment, 1}};
    Fwog::BeginRendering(shadingRenderingInfo);
    Fwog::ScopedDebugMarker marker("Shading");
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
    Fwog::Cmd::BindSampledImage(0, frame.gAlbedo.value(), nearestSampler);
    Fwog::Cmd::BindSampledImage(1, frame.gNormal.value(), nearestSampler);
    Fwog::Cmd::BindSampledImage(2, frame.gDepth.value(), nearestSampler);
    Fwog::Cmd::BindSampledImage(3, shadowDepth, shadowSampler);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(0, lightBuffer.value(), 0, lightBuffer->Size());
    Fwog::Cmd::Draw(3, 1, 0, 0);
    Fwog::EndRendering();
  }

  // volumetric fog pass
  {
    static Fwog::TimerQueryAsync timer(5);
    if (auto t = timer.PopTimestamp())
    {
      volumetricTime = *t / 10e5;
    }
    Fwog::TimerScoped scopedTimer(timer);

    Fwog::ScopedDebugMarker marker("Volumetric Fog");
    volumetric.UpdateUniforms(mainCamera,
                              proj,
                              shadingUniforms.sunViewProj,
                              shadingUniforms.sunDir,
                              fovy,
                              aspectRatio,
                              config.volumeNearPlane,
                              config.volumeFarPlane,
                              static_cast<float>(glfwGetTime()),
                              config.volumeUseScatteringTexture,
                              config.volumeAnisotropyG,
                              config.volumeNoiseOffsetScale,
                              config.frog,
                              config.volumetricGroundFogDensity,
                              sunColor * sunStrength);

    volumetric.AccumulateDensity(densityVolume, esmTex, esmUniformBuffer, lightBuffer.value());

    volumetric.MarchVolume(densityVolume, scatteringVolume);

    volumetric.ApplyDeferred(frame.shadingTexHdr.value(),
                             frame.gDepth.value(),
                             frame.shadingTexHdr.value(),
                             scatteringVolume,
                             noiseTexture.value());
  }

  {
    Fwog::ScopedDebugMarker marker("Postprocessing");
    Fwog::BeginCompute();
    Fwog::Cmd::BindComputePipeline(postprocessingPipeline);
    Fwog::Cmd::BindSampledImage(0, frame.shadingTexHdr.value(), nearestSampler);
    Fwog::Cmd::BindSampledImage(1, noiseTexture.value(), nearestSampler);
    Fwog::Cmd::BindImage(0, frame.shadingTexLdr.value(), 0);
    Fwog::Extent2D numGroups = (frame.shadingTexLdr->Extent() + 7) / 8;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, 1);
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    Fwog::EndCompute();
  }

  // on my driver (Nvidia 3.25.1.27), blitting appears to not perform automatic linear->sRGB conversions
  // hence, a full-screen triangle will instead be used
  // Fwog::BlitTextureToSwapchain(*shadingTexView,
  //  {},
  //  {},
  //  shadingTexView->Extent(),
  //  shadingTexView->Extent(),
  //  Fwog::Filter::LINEAR);

  // copy to swapchain
  {
    Fwog::SwapchainRenderInfo swapchainRenderingInfo{
      .viewport = {.drawRect{.extent = {windowWidth, windowHeight}}},
    };
    Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
    Fwog::ScopedDebugMarker marker("Copy to Swapchain");

    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

    Fwog::Texture* tex = &frame.shadingTexLdr.value();
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
      tex = &frame.gAlbedo.value();
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
      tex = &frame.gNormal.value();
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
      tex = &frame.gDepth.value();
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
      tex = &shadowDepth;

    Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
    Fwog::Cmd::BindSampledImage(0, *tex, nearestSampler);
    Fwog::Cmd::Draw(3, 1, 0, 0);
    Fwog::EndRendering();
  }
}

void VolumetricApplication::OnGui([[maybe_unused]] double dt)
{
  ImGui::Begin("Volumetric Fog");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Volumetric fog: %f ms", volumetricTime);
  ImGui::SliderFloat("Sun Angle", &sunPosition, -3.14159f, 3.14159f);
  ImGui::ColorEdit3("Sun Color", &sunColor[0], ImGuiColorEditFlags_Float);
  ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 20);

  ImGui::Separator();

  ImGui::SliderFloat("Volume near plane", &config.volumeNearPlane, 0.1f, 1.0f);
  ImGui::SliderFloat("Volume far plane", &config.volumeFarPlane, 20.0f, 500.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("ESM exponent", &config.esmExponent, 1.0f, 90.0f);
  int passes = static_cast<int>(config.esmBlurPasses);
  ImGui::SliderInt("ESM blur passes", &passes, 0, 5);
  config.esmBlurPasses = static_cast<size_t>(passes);
  ImGui::Checkbox("Use scattering texture", &config.volumeUseScatteringTexture);
  ImGui::SliderFloat("Volume anisotropy", &config.volumeAnisotropyG, -1, 1);
  ImGui::SliderFloat("Volume noise scale", &config.volumeNoiseOffsetScale, 0, 1);
  ImGui::Checkbox("Frog", &config.frog);
  ImGui::SliderFloat("Volume ground density", &config.volumetricGroundFogDensity, 0, 1);
  ImGui::End();
}

int main(int argc, const char* const* argv)
{
  std::optional<std::string_view> filename;
  float scale = 1.0f;
  bool binary = false;

  try
  {
    if (argc > 1)
    {
      filename = argv[1];
    }
    if (argc > 2)
    {
      scale = std::stof(argv[2]);
    }
    if (argc > 3)
    {
      int val = 0;
      auto [ptr, ec] = std::from_chars(argv[3], argv[3] + strlen(argv[3]), val);
      binary = static_cast<bool>(val);
      if (ec != std::errc{})
      {
        throw std::runtime_error("Binary should be 0 or 1");
      }
    }
  }
  catch (std::exception& e)
  {
    printf("Argument parsing error: %s\n", e.what());
    return -1;
  }

  auto appInfo = Application::CreateInfo{.name = "Volumetric Fog Example"};
  auto app = VolumetricApplication(appInfo, filename, scale, binary);
  app.Run();

  return 0;
}