/* volumetric.cpp
 *
 * Volumetric fog viewer.
 *
 * Takes the same command line arguments as the gltf_viewer example.
 */

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "common/common.h"

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

// not needed because SceneLoader implements stb_include
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

////////////////////////////////////// Externals

namespace ImGui
{
  extern ImGuiKeyData* GetKeyData(ImGuiKey key);
}

////////////////////////////////////// Types
struct View
{
  glm::vec3 position{};
  float pitch{}; // pitch angle in radians
  float yaw{};   // yaw angle in radians

  glm::vec3 GetForwardDir() const
  {
    return glm::vec3{cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw)};
  }

  glm::mat4 GetViewMatrix() const
  {
    return glm::lookAt(position, position + GetForwardDir(), glm::vec3(0, 1, 0));
  }
};

glm::mat4 InfReverseZPerspectiveRH(float fovY_radians, float aspectWbyH, float zNear)
{
  float f = 1.0f / tan(fovY_radians / 2.0f);
  return glm::
      mat4(f / aspectWbyH, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, zNear, 0.0f);
}

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

////////////////////////////////////// Globals
// constexpr int gWindowWidth = 1920;
// constexpr int gWindowHeight = 1080;
constexpr int gWindowWidth = 1280;
constexpr int gWindowHeight = 720;
float gPreviousCursorX = gWindowWidth / 2.0f;
float gPreviousCursorY = gWindowHeight / 2.0f;
float gCursorOffsetX = 0;
float gCursorOffsetY = 0;
float gSensitivity = 0.0025f;

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
  float volumeNoiseOffsetScale = 1.0f;
  bool frog = false;
  float volumetricGroundFogDensity = .15f;

  float lightFarPlane = 50.0f;
  float lightProjWidth = 24.0f;
  float lightDistance = 25.0f;
} config;

std::array<Fwog::VertexInputBindingDescription, 3> GetSceneInputBindingDescs()
{
  Fwog::VertexInputBindingDescription descPos{
      .location = 0,
      .binding = 0,
      .format = Fwog::Format::R32G32B32_FLOAT,
      .offset = offsetof(Utility::Vertex, position),
  };
  Fwog::VertexInputBindingDescription descNormal{
      .location = 1,
      .binding = 0,
      .format = Fwog::Format::R16G16_SNORM,
      .offset = offsetof(Utility::Vertex, normal),
  };
  Fwog::VertexInputBindingDescription descUV{
      .location = 2,
      .binding = 0,
      .format = Fwog::Format::R32G32_FLOAT,
      .offset = offsetof(Utility::Vertex, texcoord),
  };

  return {descPos, descNormal, descUV};
}

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Utility::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Utility::LoadFile("shaders/SceneDeferredSimple.frag.glsl"));

  return Fwog::GraphicsPipeline({
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .vertexInputState = {GetSceneInputBindingDescs()},
      .depthState = {.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::GREATER},
  });
}

Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Utility::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));

  return Fwog::GraphicsPipeline({
      .vertexShader = &vertexShader,
      .vertexInputState = {GetSceneInputBindingDescs()},
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
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Utility::LoadFile("shaders/ShadeDeferredSimple.frag.glsl"));

  return Fwog::GraphicsPipeline({
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
      .depthState = {.depthTestEnable = false, .depthWriteEnable = false},
  });
}

Fwog::GraphicsPipeline CreateDebugTexturePipeline()
{
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Utility::LoadFile("shaders/Texture.frag.glsl"));

  return Fwog::GraphicsPipeline({
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
      .depthState = {.depthTestEnable = false, .depthWriteEnable = false},
  });
}

Fwog::ComputePipeline CreateCopyToEsmPipeline()
{
  auto shader = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER,
                             Utility::LoadFile("shaders/volumetric/Depth2exponential.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &shader});
}

Fwog::ComputePipeline CreateGaussianBlurPipeline()
{
  auto shader =
      Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Utility::LoadFile("shaders/volumetric/GaussianBlur.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &shader});
}

Fwog::ComputePipeline CreatePostprocessingPipeline()
{
  auto shader = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER,
                             Utility::LoadFile("shaders/volumetric/TonemapAndDither.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &shader});
}

void CursorPosCallback([[maybe_unused]] GLFWwindow* window, double currentCursorX, double currentCursorY)
{
  ImGui_ImplGlfw_CursorPosCallback(window, currentCursorX, currentCursorY);
  static bool firstFrame = true;
  if (firstFrame)
  {
    gPreviousCursorX = static_cast<float>(currentCursorX);
    gPreviousCursorY = static_cast<float>(currentCursorY);
    firstFrame = false;
  }

  gCursorOffsetX += static_cast<float>(currentCursorX) - gPreviousCursorX;
  gCursorOffsetY += gPreviousCursorY - static_cast<float>(currentCursorY);
  gPreviousCursorX = static_cast<float>(currentCursorX);
  gPreviousCursorY = static_cast<float>(currentCursorY);
}

View ProcessMovement(GLFWwindow* window, View camera, float dt)
{
  constexpr float speed = 4.5f;
  const glm::vec3 forward = camera.GetForwardDir();
  const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.position += forward * dt * speed;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.position -= forward * dt * speed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.position += right * dt * speed;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.position -= right * dt * speed;
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.position.y -= dt * speed;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.position.y += dt * speed;

  camera.yaw += gCursorOffsetX * gSensitivity;
  camera.pitch += gCursorOffsetY * gSensitivity;
  camera.pitch = glm::clamp(camera.pitch, -glm::half_pi<float>() + 1e-4f, glm::half_pi<float>() - 1e-4f);

  return camera;
}

class Volumetric
{
public:
  void Init()
  {
    char error[256] = {};
    char* accumulateDensity =
        stb_include_string(Utility::LoadFile("shaders/volumetric/CellLightingAndDensity.comp.glsl").data(),
                           nullptr,
                           "shaders/volumetric",
                           "CellLightingAndDensity",
                           error);

    char* marchVolume = stb_include_string(Utility::LoadFile("shaders/volumetric/MarchVolume.comp.glsl").data(),
                                           nullptr,
                                           "shaders/volumetric",
                                           "marchVolume",
                                           error);

    char* applyDeferred =
        stb_include_string(Utility::LoadFile("shaders/volumetric/ApplyVolumetricsDeferred.comp.glsl").data(),
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
    // This texture is used if a compile-time switch is set in marchVolume.comp.glsl.
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

    scatteringTexture = Fwog::Texture(Fwog::TextureCreateInfo{.imageType = Fwog::ImageType::TEX_1D,
                                                              .format = Fwog::Format::R16G16B16_FLOAT,
                                                              .extent = {static_cast<uint32_t>(data.size())},
                                                              .mipLevels = 1,
                                                              .arrayLayers = 1,
                                                              .sampleCount = Fwog::SampleCount::SAMPLES_1});

    scatteringTexture->SubImage({.dimension = Fwog::UploadDimension::ONE,
                                 .size = {static_cast<uint32_t>(data.size())},
                                 .format = Fwog::UploadFormat::RGB,
                                 .type = Fwog::UploadType::FLOAT,
                                 .pixels = data.data()});
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
                      float groundFogDensity)
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
                .groundFogDensity = groundFogDensity};

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

void RenderScene(std::optional<std::string_view> fileName, float scale, bool binary)
{
  GLFWwindow* window = Utility::CreateWindow({.name = "Volumetric Fog Example",
                                              .maximize = false,
                                              .decorate = true,
                                              .width = gWindowWidth,
                                              .height = gWindowHeight});
  Utility::InitOpenGL();

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsDark();
  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  // create gbuffer textures and render info
  auto gBufferColorTexture = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::R8G8B8A8_UNORM);
  auto gBufferNormalTexture = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::R16G16B16_SNORM);
  auto gBufferDepthTexture = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::D32_FLOAT);

  // create shadow depth texture and render info
  auto shadowDepthTexture = Fwog::CreateTexture2D(config.shadowmapResolution, Fwog::Format::D16_UNORM);

  auto shadingTex = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::R16G16B16A16_FLOAT);

  Utility::Scene scene;

  if (!fileName)
  {
    Utility::LoadModelFromFile(scene, "models/simple_scene.glb", glm::mat4{.5}, true);
  }
  else
  {
    Utility::LoadModelFromFile(scene, *fileName, glm::scale(glm::vec3{scale}), binary);
  }

  std::vector<ObjectUniforms> meshUniforms;
  for (size_t i = 0; i < scene.meshes.size(); i++)
  {
    meshUniforms.push_back({scene.meshes[i].transform});
  }

  ShadingUniforms shadingUniforms{
      .sunDir = glm::normalize(glm::vec4{-.1, -.3, -.6, 0}),
      .sunStrength = glm::vec4{3, 3, 3, 0},
  };

  //////////////////////////////////////// Clustered rendering stuff
  std::vector<Light> lights;
  lights.push_back(Light{.position = {-3, 1, -1, 0}, .intensity = {.2f, .8f, 1.0f}, .invRadius = 1.0f / 4.0f});
  lights.push_back(Light{.position = {3, 2, 0, 0}, .intensity = {.7f, .8f, 0.1f}, .invRadius = 1.0f / 2.0f});
  lights.push_back(Light{.position = {3, 3, 2, 0}, .intensity = {1.2f, .8f, .1f}, .invRadius = 1.0f / 6.0f});
  lights.push_back(Light{.position = {.9, 5.5, -1.65, 0}, .intensity = {5.2f, 4.8f, 12.5f}, .invRadius = 1.0f / 9.0f});

  // auto globalUniformsBuffer = Fwog::Buffer(sizeof(GlobalUniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  auto globalUniformsBuffer =
      Fwog::TypedBuffer<GlobalUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE, Fwog::BufferMapFlag::MAP_WRITE);
  auto shadingUniformsBuffer = Fwog::Buffer(shadingUniforms, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  auto materialUniformsBuffer = Fwog::Buffer(sizeof(Utility::GpuMaterial), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  auto meshUniformBuffer = Fwog::Buffer(std::span(meshUniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  auto lightBuffer = Fwog::Buffer(std::span(lights), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

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

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();
  Fwog::GraphicsPipeline shadowPipeline = CreateShadowPipeline();
  Fwog::GraphicsPipeline shadingPipeline = CreateShadingPipeline();
  Fwog::GraphicsPipeline debugTexturePipeline = CreateDebugTexturePipeline();
  Fwog::ComputePipeline copyToEsmPipeline = CreateCopyToEsmPipeline();
  Fwog::ComputePipeline gaussianBlurPipeline = CreateGaussianBlurPipeline();
  Fwog::ComputePipeline postprocessingPipeline = CreatePostprocessingPipeline();

  View camera;
  camera.position = {0, 1.5, 2};
  camera.yaw = -glm::half_pi<float>();

  const auto fovy = glm::radians(70.f);
  const auto aspectRatio = gWindowWidth / (float)gWindowHeight;
  // auto proj = glm::perspective(fovy, aspectRatio, 0.1f, 100.f);
  auto proj = InfReverseZPerspectiveRH(fovy, aspectRatio, config.viewNearPlane);

  auto volumeInfo = Fwog::TextureCreateInfo{.imageType = Fwog::ImageType::TEX_3D,
                                            .format = Fwog::Format::R16G16B16A16_FLOAT,
                                            .extent = config.volumeExtent,
                                            .mipLevels = 1,
                                            .arrayLayers = 1,
                                            .sampleCount = Fwog::SampleCount::SAMPLES_1};
  auto densityVolume = Fwog::Texture(volumeInfo);
  auto scatteringVolume = Fwog::Texture(volumeInfo);

  int x = 0;
  int y = 0;
  auto noise = stbi_load("textures/bluenoise32.png", &x, &y, nullptr, 4);
  assert(noise);
  auto noiseTexture =
      Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
  noiseTexture.SubImage({.dimension = Fwog::UploadDimension::TWO,
                         .level = 0,
                         .offset = {},
                         .size = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
                         .format = Fwog::UploadFormat::RGBA,
                         .type = Fwog::UploadType::UBYTE,
                         .pixels = noise});
  stbi_image_free(noise);

  Volumetric volumetric;
  volumetric.Init();

  auto exponentialShadowMap = Fwog::CreateTexture2D(config.esmResolution, Fwog::Format::R32_FLOAT);
  auto exponentialShadowMapIntermediate = Fwog::CreateTexture2D(config.esmResolution, Fwog::Format::R32_FLOAT);
  auto esmUniformBuffer = Fwog::Buffer(sizeof(float), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  auto esmBlurUniformBuffer = Fwog::Buffer(sizeof(glm::ivec2) * 2, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  auto ldrSceneColorTex = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::R8G8B8A8_UNORM);

  bool cursorIsActive = false;

  float prevFrame = static_cast<float>(glfwGetTime());
  while (!glfwWindowShouldClose(window))
  {
    // calculate dt
    float curFrame = static_cast<float>(glfwGetTime());
    float dt = curFrame - prevFrame;
    prevFrame = curFrame;

    // process input
    gCursorOffsetX = 0;
    gCursorOffsetY = 0;
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }
    if (ImGui::GetKeyData(static_cast<ImGuiKey>(GLFW_KEY_GRAVE_ACCENT))->DownDuration == 0.0f)
    {
      cursorIsActive = !cursorIsActive;
      glfwSetInputMode(window, GLFW_CURSOR, cursorIsActive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    // hack to prevent the "disabled" (but actually just invisible) cursor from being able to click stuff in ImGui
    if (!cursorIsActive)
    {
      glfwSetCursorPos(window, 0, 0);
      gPreviousCursorX = 0;
      gPreviousCursorY = 0;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Options");
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

    if (!cursorIsActive)
    {
      camera = ProcessMovement(window, camera, dt);
    }

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{1, 0, 0}) * shadingUniforms.sunDir;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{-1, 0, 0}) * shadingUniforms.sunDir;
    }

    // update global uniforms
    GlobalUniforms mainCameraUniforms{};
    mainCameraUniforms.viewProj = proj * camera.GetViewMatrix();
    mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
    mainCameraUniforms.cameraPos = glm::vec4(camera.position, 0.0);
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
      Fwog::RenderAttachment gcolorAttachment{.texture = &gBufferColorTexture,
                                              .clearValue = Fwog::ClearColorValue{.1f, .3f, .5f, 0.0f},
                                              .clearOnLoad = true};
      Fwog::RenderAttachment gnormalAttachment{.texture = &gBufferNormalTexture,
                                               .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
                                               .clearOnLoad = false};
      Fwog::RenderAttachment gdepthAttachment{.texture = &gBufferDepthTexture,
                                              .clearValue = Fwog::ClearDepthStencilValue{.depth = 0.0f},
                                              .clearOnLoad = true};
      Fwog::RenderAttachment cgAttachments[] = {gcolorAttachment, gnormalAttachment};
      Fwog::RenderInfo gbufferRenderInfo{.colorAttachments = cgAttachments,
                                         .depthAttachment = &gdepthAttachment,
                                         .stencilAttachment = nullptr};
      Fwog::BeginRendering(gbufferRenderInfo);
      Fwog::ScopedDebugMarker marker("Geometry");
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer, 0, materialUniformsBuffer.Size());

      Fwog::Cmd::BindStorageBuffer(1, meshUniformBuffer, 0, meshUniformBuffer.Size());
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
      Fwog::RenderAttachment depthAttachment{.texture = &shadowDepthTexture,
                                             .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
                                             .clearOnLoad = true};

      Fwog::RenderInfo shadowRenderInfo{.depthAttachment = &depthAttachment, .stencilAttachment = nullptr};
      Fwog::BeginRendering(shadowRenderInfo);
      Fwog::ScopedDebugMarker marker("Shadow Scene");
      Fwog::Cmd::BindGraphicsPipeline(shadowPipeline);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(1, meshUniformBuffer, 0, meshUniformBuffer.Size());

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
          Fwog::Cmd::BindSampledImage(0, shadowDepthTexture, nearestMirrorSampler);
          Fwog::Cmd::BindImage(0, exponentialShadowMap, 0);
          Fwog::Cmd::BindUniformBuffer(0, esmUniformBuffer, 0, esmUniformBuffer.Size());
          auto dispatchDim = (exponentialShadowMap.Extent() + 7) / 8;
          Fwog::Cmd::Dispatch(dispatchDim.width, dispatchDim.height, 1);

          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        }

        // blur
        {
          Fwog::ScopedDebugMarker marker("Blur ESM");
          Fwog::Cmd::BindComputePipeline(gaussianBlurPipeline);

          auto linearSampler = Fwog::Sampler({.minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

          struct
          {
            glm::ivec2 direction;
            glm::ivec2 targetDim;
          } esmBlurUniforms;

          const auto esmExtent1 = exponentialShadowMap.Extent();
          const auto esmExtent2 = exponentialShadowMapIntermediate.Extent();

          const auto dispatchSize1 = (esmExtent2 + 7) / 8;
          const auto dispatchSize2 = (esmExtent1 + 7) / 8;

          Fwog::Cmd::BindUniformBuffer(0, esmBlurUniformBuffer, 0, esmBlurUniformBuffer.Size());

          for (size_t i = 0; i < config.esmBlurPasses; i++)
          {
            esmBlurUniforms.direction = {0, 1};
            esmBlurUniforms.targetDim = {esmExtent2.width, esmExtent2.height};
            esmBlurUniformBuffer.SubData(esmBlurUniforms, 0);
            Fwog::Cmd::BindSampledImage(0, exponentialShadowMap, linearSampler);
            Fwog::Cmd::BindImage(0, exponentialShadowMapIntermediate, 0);
            Fwog::Cmd::Dispatch(dispatchSize1.width, dispatchSize1.height, 1);

            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

            esmBlurUniforms.direction = {1, 0};
            esmBlurUniforms.targetDim = {esmExtent1.width, esmExtent1.height};
            esmBlurUniformBuffer.SubData(esmBlurUniforms, 0);
            Fwog::Cmd::BindSampledImage(0, exponentialShadowMapIntermediate, linearSampler);
            Fwog::Cmd::BindImage(0, exponentialShadowMap, 0);
            Fwog::Cmd::Dispatch(dispatchSize2.width, dispatchSize2.height, 1);

            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          }
        }
      }
      Fwog::EndCompute();
    }

    globalUniformsBuffer.SubData(mainCameraUniforms, 0);

    // shading pass (full screen tri)
    {
      Fwog::RenderAttachment shadingAttachment{.texture = &shadingTex, .clearOnLoad = false};

      Fwog::RenderInfo shadingRenderingInfo{.colorAttachments = {&shadingAttachment, 1}};
      Fwog::BeginRendering(shadingRenderingInfo);
      Fwog::ScopedDebugMarker marker("Shading");
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, gBufferColorTexture, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, gBufferNormalTexture, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, gBufferDepthTexture, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, shadowDepthTexture, shadowSampler);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(0, lightBuffer, 0, lightBuffer.Size());
      Fwog::Cmd::Draw(3, 1, 0, 0);
      Fwog::EndRendering();
    }

    // volumetric fog pass
    {
      Fwog::ScopedDebugMarker marker("Volumetric Fog");
      volumetric.UpdateUniforms(camera,
                                proj,
                                shadingUniforms.sunViewProj,
                                shadingUniforms.sunDir,
                                fovy,
                                aspectRatio,
                                config.volumeNearPlane,
                                config.volumeFarPlane,
                                curFrame,
                                config.volumeUseScatteringTexture,
                                config.volumeAnisotropyG,
                                config.volumeNoiseOffsetScale,
                                config.frog,
                                config.volumetricGroundFogDensity);

      volumetric.AccumulateDensity(densityVolume, exponentialShadowMap, esmUniformBuffer, lightBuffer);

      volumetric.MarchVolume(densityVolume, scatteringVolume);

      volumetric.ApplyDeferred(shadingTex, gBufferDepthTexture, shadingTex, scatteringVolume, noiseTexture);
    }

    {
      Fwog::ScopedDebugMarker marker("Postprocessing");
      Fwog::BeginCompute();
      Fwog::Cmd::BindComputePipeline(postprocessingPipeline);
      Fwog::Cmd::BindSampledImage(0, shadingTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, noiseTexture, nearestSampler);
      Fwog::Cmd::BindImage(0, ldrSceneColorTex, 0);
      Fwog::Extent2D numGroups = (ldrSceneColorTex.Extent() + 7) / 8;
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
          .viewport = {.drawRect{.extent = {gWindowWidth, gWindowHeight}}},
      };
      Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
      Fwog::ScopedDebugMarker marker("Copy to Swapchain");

      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      Fwog::Texture* tex = &ldrSceneColorTex;

      if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        tex = &gBufferColorTexture;
      if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        tex = &gBufferNormalTexture;
      if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        tex = &gBufferDepthTexture;
      if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        tex = &shadowDepthTexture;

      Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
      Fwog::Cmd::BindSampledImage(0, *tex, nearestSampler);
      Fwog::Cmd::Draw(3, 1, 0, 0);
      Fwog::EndRendering();
    }

    ImGui::Render();
    {
      auto marker = Fwog::ScopedDebugMarker("Draw GUI");
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    ImGui::EndFrame();

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
}

int main(int argc, const char* const* argv)
{
  std::optional<std::string_view> fileName;
  float scale = 1.0f;
  bool binary = false;

  try
  {
    if (argc > 1)
    {
      fileName = argv[1];
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

  RenderScene(fileName, scale, binary);

  return 0;
}