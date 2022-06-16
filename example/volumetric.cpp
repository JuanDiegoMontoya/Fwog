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

#include <fwog/BasicTypes.h>
#include <fwog/Rendering.h>
#include <fwog/Pipeline.h>
#include <fwog/DebugMarker.h>
#include <fwog/Timer.h>
#include <fwog/Texture.h>
#include <fwog/Buffer.h>
#include <fwog/Shader.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>

#include "common/SceneLoader.h"

#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>

// not needed because SceneLoader implements stb_include
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

////////////////////////////////////// Types
struct View
{
  glm::vec3 position{};
  float pitch{}; // pitch angle in radians
  float yaw{};   // yaw angle in radians

  glm::vec3 GetForwardDir() const
  {
    return glm::vec3
    {
      cos(pitch) * cos(yaw),
      sin(pitch),
      cos(pitch) * sin(yaw)
    };
  }

  glm::mat4 GetViewMatrix() const
  {
    return glm::lookAt(position, position + GetForwardDir(), glm::vec3(0, 1, 0));
  }

  void SetForwardDir(glm::vec3 dir)
  {
    assert(glm::abs(1.0f - glm::length(dir)) < 0.0001f);
    pitch = glm::asin(dir.y);
    yaw = glm::acos(dir.x / glm::cos(pitch));
    if (dir.x >= 0 && dir.z < 0)
      yaw *= -1;
  }
};

glm::mat4 InfReverseZPerspectiveRH(float fovY_radians, float aspectWbyH, float zNear)
{
  float f = 1.0f / tan(fovY_radians / 2.0f);
  return glm::mat4(
    f / aspectWbyH, 0.0f, 0.0f, 0.0f,
    0.0f, f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, zNear, 0.0f);
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
//constexpr int gWindowWidth = 1920;
//constexpr int gWindowHeight = 1080;
constexpr int gWindowWidth = 1280;
constexpr int gWindowHeight = 720;
float gPreviousCursorX = gWindowWidth / 2.0f;
float gPreviousCursorY = gWindowHeight / 2.0f;
float gCursorOffsetX = 0;
float gCursorOffsetY = 0;
float gSensitivity = 0.005f;

struct
{
  uint32_t shadowmapWidth = 1024;
  uint32_t shadowmapHeight = 1024;

  float esmExponent = 15.0f;
  size_t esmBlurPasses = 1;
}constexpr config;

std::array<Fwog::VertexInputBindingDescription, 3> GetSceneInputBindingDescs()
{
  Fwog::VertexInputBindingDescription descPos
  {
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Utility::Vertex, position),
  };
  Fwog::VertexInputBindingDescription descNormal
  {
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R16G16_SNORM,
    .offset = offsetof(Utility::Vertex, normal),
  };
  Fwog::VertexInputBindingDescription descUV
  {
    .location = 2,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = offsetof(Utility::Vertex, texcoord),
  };

  return { descPos, descNormal, descUV };
}

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vertexShader = Fwog::Shader::Create(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));
  auto fragmentShader = Fwog::Shader::Create(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/SceneDeferredSimple.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader.value(),
      .fragmentShader = &fragmentShader.value(),
      .vertexInputState = GetSceneInputBindingDescs(),
      .depthState = { .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::GREATER }
    });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vertexShader = Fwog::Shader::Create(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/SceneDeferredSimple.vert.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader.value(),
      .vertexInputState = GetSceneInputBindingDescs(),
      .rasterizationState =
      {
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 3.0f,
        .depthBiasSlopeFactor = 5.0f,
      },
      .depthState = {.depthTestEnable = true, .depthWriteEnable = true }
    });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vertexShader = Fwog::Shader::Create(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader = Fwog::Shader::Create(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/ShadeDeferredSimple.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader.value(),
      .fragmentShader = &fragmentShader.value(),
      .rasterizationState = {.cullMode = Fwog::CullMode::NONE },
      .depthState = {.depthTestEnable = false, .depthWriteEnable = false }
    });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

Fwog::GraphicsPipeline CreateDebugTexturePipeline()
{
  auto vertexShader = Fwog::Shader::Create(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fragmentShader = Fwog::Shader::Create(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/Texture.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader.value(),
      .fragmentShader = &fragmentShader.value(),
      .rasterizationState = {.cullMode = Fwog::CullMode::NONE },
      .depthState = {.depthTestEnable = false, .depthWriteEnable = false }
    });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

Fwog::ComputePipeline CreateCopyToEsmPipeline()
{
  auto shader = Fwog::Shader::Create(
    Fwog::PipelineStage::COMPUTE_SHADER,
    Utility::LoadFile("shaders/volumetric/depth2exponential.comp.glsl"));

  auto pipeline = Fwog::CompileComputePipeline({ .shader = &shader.value() });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

Fwog::ComputePipeline CreateGaussianBlurPipeline()
{
  auto shader = Fwog::Shader::Create(
    Fwog::PipelineStage::COMPUTE_SHADER,
    Utility::LoadFile("shaders/volumetric/gaussianBlur.comp.glsl"));

  auto pipeline = Fwog::CompileComputePipeline({ .shader = &shader.value() });

  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

void CursorPosCallback(GLFWwindow* window, double currentCursorX, double currentCursorY)
{
  static bool firstFrame = true;
  if (firstFrame)
  {
    gPreviousCursorX = static_cast<float>(currentCursorX);
    gPreviousCursorY = static_cast<float>(currentCursorY);
    firstFrame = false;
  }

  gCursorOffsetX = static_cast<float>(currentCursorX) - gPreviousCursorX;
  gCursorOffsetY = gPreviousCursorY - static_cast<float>(currentCursorY);
  gPreviousCursorX = static_cast<float>(currentCursorX);
  gPreviousCursorY = static_cast<float>(currentCursorY);
}

View ProcessMovement(GLFWwindow* window, View camera, float dt)
{
  constexpr float speed = 4.5f;
  const glm::vec3 forward = camera.GetForwardDir();
  const glm::vec3 right = glm::normalize(glm::cross(forward, { 0, 1, 0 }));

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
    char* accumulateDensity = stb_include_string(
      Utility::LoadFile("shaders/volumetric/accumulateDensity.comp.glsl").data(),
      nullptr,
      const_cast<char*>("shaders/volumetric"),
      const_cast<char*>("accumulateDensity"),
      error);

    char* marchVolume = stb_include_string(
      Utility::LoadFile("shaders/volumetric/marchVolume.comp.glsl").data(),
      nullptr,
      const_cast<char*>("shaders/volumetric"),
      const_cast<char*>("marchVolume"),
      error);

    char* applyDeferred = stb_include_string(
      Utility::LoadFile("shaders/volumetric/applyDeferred.comp.glsl").data(),
      nullptr,
      const_cast<char*>("shaders/volumetric"),
      const_cast<char*>("applyDeferred"),
      error);

    std::string infoLog;
    auto accumulateShader = Fwog::Shader::Create(
      Fwog::PipelineStage::COMPUTE_SHADER,
      accumulateDensity,
      &infoLog);
    auto marchShader = Fwog::Shader::Create(
      Fwog::PipelineStage::COMPUTE_SHADER,
      marchVolume);
    auto applyShader = Fwog::Shader::Create(
      Fwog::PipelineStage::COMPUTE_SHADER,
      applyDeferred);
    
    free(applyDeferred);
    free(marchVolume);
    free(accumulateDensity);

    accumulateDensityPipeline = *Fwog::CompileComputePipeline({ .shader = &accumulateShader.value() });
    marchVolumePipeline = *Fwog::CompileComputePipeline({ .shader = &marchShader.value() });
    applyDeferredPipeline = *Fwog::CompileComputePipeline({ .shader = &applyShader.value() });
  }

  void UpdateUniforms(const View& view, 
    const glm::mat4& projCamera, 
    const glm::mat4& sunViewProj,
    glm::vec3 sunDir, 
    float fovy, 
    float aspectRatio, 
    float volumeNearPlane, 
    float volumeFarPlane, 
    float time)
  {
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
    }uniforms;

    glm::mat4 projVolume = glm::perspectiveZO(fovy, aspectRatio, volumeNearPlane, volumeFarPlane);
    glm::mat4 viewMat = view.GetViewMatrix();
    glm::mat4 viewProjVolume = projVolume * viewMat;
    
    uniforms =
    {
      .viewPos = view.position,
      .time = time,
      .invViewProjScene = glm::inverse(projCamera * viewMat),
      .viewProjVolume = viewProjVolume,
      .invViewProjVolume = glm::inverse(viewProjVolume),
      .sunViewProj = sunViewProj,
      .sunDir = sunDir,
      .volumeNearPlane = volumeNearPlane,
      .volumeFarPlane = volumeFarPlane
    };

    if (!uniformBuffer)
    {
      uniformBuffer = Fwog::Buffer::Create(sizeof(uniforms), Fwog::BufferFlag::DYNAMIC_STORAGE);
    }
    uniformBuffer->SubData(uniforms, 0);
  }

  void AccumulateDensity(const Fwog::TextureView densityVolume)
  {
    assert(densityVolume.CreateInfo().viewType == Fwog::ImageType::TEX_3D);

    Fwog::BeginCompute();
    Fwog::Cmd::BindComputePipeline(accumulateDensityPipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindImage(0, densityVolume, 0);
    Fwog::Extent3D numGroups = (densityVolume.Extent() + 7) / 8;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, numGroups.depth);
    Fwog::EndCompute();
  }

  void MarchVolume(
    const Fwog::TextureView& sourceVolume,
    const Fwog::TextureView& targetVolume,
    const Fwog::TextureView& shadowDepth,
    const Fwog::Buffer& esmUniformBuffer,
    const Fwog::Buffer& lightBuffer)
  {
    assert(sourceVolume.CreateInfo().viewType == Fwog::ImageType::TEX_3D);
    assert(targetVolume.CreateInfo().viewType == Fwog::ImageType::TEX_3D);

    auto sampler = Fwog::TextureSampler::Create({ .minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR});

    //auto shadowSampler = Fwog::TextureSampler::Create({ .compareEnable = true, .compareOp = Fwog::CompareOp::LESS });

    Fwog::BeginCompute();
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::BindComputePipeline(marchVolumePipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindUniformBuffer(1, esmUniformBuffer, 0, esmUniformBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(0, lightBuffer, 0, lightBuffer.Size());
    Fwog::Cmd::BindSampledImage(0, sourceVolume, *sampler);
    Fwog::Cmd::BindSampledImage(1, shadowDepth, *sampler);
    Fwog::Cmd::BindImage(0, targetVolume, 0);
    Fwog::Extent3D numGroups = (targetVolume.Extent() + 15) / 16;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, 1);
    Fwog::EndCompute();
  }

  void ApplyDeferred(
    const Fwog::TextureView& gbufferColor,
    const Fwog::TextureView& gbufferDepth,
    const Fwog::TextureView& targetColor,
    const Fwog::TextureView& sourceVolume,
    const Fwog::TextureView& noise)
  {
    assert(sourceVolume.CreateInfo().viewType == Fwog::ImageType::TEX_3D);
    assert(targetColor.Extent() == gbufferColor.Extent() && targetColor.Extent() == gbufferDepth.Extent());

    auto sampler = Fwog::TextureSampler::Create({ .minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR });

    Fwog::Extent3D targetDim = targetColor.Extent();

    Fwog::BeginCompute();
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::BindComputePipeline(applyDeferredPipeline);
    Fwog::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    Fwog::Cmd::BindSampledImage(0, gbufferColor, *sampler);
    Fwog::Cmd::BindSampledImage(1, gbufferDepth, *sampler);
    Fwog::Cmd::BindSampledImage(2, sourceVolume, *sampler);
    Fwog::Cmd::BindSampledImage(3, noise, *sampler);
    Fwog::Cmd::BindImage(0, targetColor, 0);
    Fwog::Extent2D numGroups = (targetColor.Extent() + 15) / 16;
    Fwog::Cmd::Dispatch(numGroups.width, numGroups.height, 1);
    Fwog::EndCompute();
  }

private:

  Fwog::ComputePipeline accumulateDensityPipeline;
  Fwog::ComputePipeline marchVolumePipeline;
  Fwog::ComputePipeline applyDeferredPipeline;
  std::optional<Fwog::Buffer> uniformBuffer;
};

void RenderScene(std::optional<std::string_view> fileName, float scale, bool binary)
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "Volumetric Fog Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  Fwog::Viewport mainViewport { .drawRect {.extent = { gWindowWidth, gWindowHeight } } };

  Fwog::Viewport shadowViewport { .drawRect { .extent = { config.shadowmapWidth, config.shadowmapHeight } } };

  // create gbuffer textures and render info
  auto gcolorTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R8G8B8A8_UNORM);
  auto gnormalTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R16G16B16_SNORM);
  auto gdepthTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::D32_FLOAT);
  auto gbufferColorView = gcolorTex->View();
  auto gbufferNormalView = gnormalTex->View();
  auto gbufferDepthView = gdepthTex->View();

  // create shadow depth texture and render info
  auto shadowDepthTex = Fwog::CreateTexture2D({ config.shadowmapWidth, config.shadowmapHeight }, Fwog::Format::D16_UNORM);
  auto shadowDepthTexView = shadowDepthTex->View();

  auto shadingTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R8G8B8A8_UNORM);
  auto shadingTexView = shadingTex->View();

  Utility::Scene scene;

  if (!fileName)
  {
    bool success = Utility::LoadModelFromFile(scene, "models/simple_scene.glb", glm::mat4{ .5 }, true);
  }
  else
  {
    bool success = Utility::LoadModelFromFile(scene, *fileName, glm::scale(glm::vec3{ scale }), binary);
  }

  std::vector<ObjectUniforms> meshUniforms;
  for (size_t i = 0; i < scene.meshes.size(); i++)
  {
    meshUniforms.push_back({ scene.meshes[i].transform });
  }

  ShadingUniforms shadingUniforms
  {
    .sunDir = glm::normalize(glm::vec4{ -.1, -.3, -.6, 0 }),
    .sunStrength = glm::vec4{ 2, 2, 2, 0 },
  };

  //////////////////////////////////////// Clustered rendering stuff
  std::vector<Light> lights;
  lights.push_back(Light{ .position = { -3, 1, -1, 0 }, .intensity = { .2f, .8f, 1.0f }, .invRadius = 1.0f / 4.0f });
  lights.push_back(Light{ .position = { 3, 2, 0, 0 }, .intensity = { .7f, .8f, 0.1f }, .invRadius = 1.0f / 2.0f });
  lights.push_back(Light{ .position = { 3, 3, 2, 0 }, .intensity = { 1.2f, .8f, .1f }, .invRadius = 1.0f / 6.0f });

  auto globalUniformsBuffer = Fwog::Buffer::Create(sizeof(GlobalUniforms), Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto shadingUniformsBuffer = Fwog::Buffer::Create(shadingUniforms, Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto materialUniformsBuffer = Fwog::Buffer::Create(sizeof(Utility::GpuMaterial), Fwog::BufferFlag::DYNAMIC_STORAGE);

  auto meshUniformBuffer = Fwog::Buffer::Create(std::span(meshUniforms), Fwog::BufferFlag::DYNAMIC_STORAGE);

  auto lightBuffer = Fwog::Buffer::Create(std::span(lights), Fwog::BufferFlag::DYNAMIC_STORAGE);

  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::TextureSampler::Create(ss);

  ss.compareEnable = true;
  ss.compareOp = Fwog::CompareOp::LESS_OR_EQUAL;
  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  auto shadowSampler = Fwog::TextureSampler::Create(ss);

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();
  Fwog::GraphicsPipeline shadowPipeline = CreateShadowPipeline();
  Fwog::GraphicsPipeline shadingPipeline = CreateShadingPipeline();
  Fwog::GraphicsPipeline debugTexturePipeline = CreateDebugTexturePipeline();
  Fwog::ComputePipeline copyToEsmPipeline = CreateCopyToEsmPipeline();
  Fwog::ComputePipeline gaussianBlurPipeline = CreateGaussianBlurPipeline();

  View camera;
  camera.position = { 0, 1.5, 2 };
  camera.yaw = -glm::half_pi<float>();

  const auto fovy = glm::radians(70.f);
  const auto aspectRatio = gWindowWidth / (float)gWindowHeight;
  //auto proj = glm::perspective(fovy, aspectRatio, 0.1f, 100.f);
  auto proj = InfReverseZPerspectiveRH(fovy, aspectRatio, 0.3f);

  auto volumeInfo = Fwog::TextureCreateInfo
  {
    .imageType = Fwog::ImageType::TEX_3D,
    .format = Fwog::Format::R16G16B16A16_FLOAT,
    .extent = { 160, 90, 256 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .sampleCount = Fwog::SampleCount::SAMPLES_1
  };
  auto densityVolume = Fwog::Texture::Create(volumeInfo);
  auto scatteringVolume = Fwog::Texture::Create(volumeInfo);
  auto densityVolumeView = densityVolume->View();
  auto scatteringVolumeView = scatteringVolume->View();
  int x = 0;
  int y = 0;
  auto noise = stbi_load("textures/bluenoise32.png", &x, &y, nullptr, 4);
  assert(noise);
  auto noiseTexture = Fwog::CreateTexture2D({ static_cast<uint32_t>(x), static_cast<uint32_t>(y) }, Fwog::Format::R8G8B8A8_UNORM);
  noiseTexture->SubImage({
      .dimension = Fwog::UploadDimension::TWO,
      .level = 0,
      .offset = {},
      .size = { static_cast<uint32_t>(x), static_cast<uint32_t>(y) },
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .pixels = noise });
  stbi_image_free(noise);
  auto noiseTextureView = noiseTexture->View();

  Volumetric volumetric;
  volumetric.Init();

  Fwog::Extent3D esmResolution = { 256, 256 };
  auto exponentialShadowMap = Fwog::CreateTexture2D(esmResolution, Fwog::Format::R32_FLOAT);
  auto exponentialShadowMapIntermediate = Fwog::CreateTexture2D(esmResolution, Fwog::Format::R32_FLOAT);
  auto esmTexView = exponentialShadowMap->View();
  auto esmIntermediateTexView = exponentialShadowMapIntermediate->View();
  auto esmUniformBuffer = Fwog::Buffer::Create(sizeof(float), Fwog::BufferFlag::DYNAMIC_STORAGE);
  auto esmBlurUniformBuffer = Fwog::Buffer::Create(sizeof(glm::ivec2) * 2, Fwog::BufferFlag::DYNAMIC_STORAGE);

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

    camera = ProcessMovement(window, camera, dt);

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ 1, 0, 0 }) * shadingUniforms.sunDir;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ -1, 0, 0 }) * shadingUniforms.sunDir;
    }

    // update global uniforms
    GlobalUniforms mainCameraUniforms{};
    mainCameraUniforms.viewProj = proj * camera.GetViewMatrix();
    mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
    mainCameraUniforms.cameraPos = glm::vec4(camera.position, 0.0);
    globalUniformsBuffer->SubData(mainCameraUniforms, 0);

    glm::vec3 eye = glm::vec3{ shadingUniforms.sunDir * -10.f };
    float eyeWidth = 9.0f;
    shadingUniforms.sunViewProj =
      glm::orthoZO(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, 0.f, 20.f) *
      glm::lookAt(eye, glm::vec3(0), glm::vec3{ 0, 1, 0 });
    shadingUniformsBuffer->SubData(shadingUniforms, 0);

    // geometry buffer pass
    {
      Fwog::RenderAttachment gcolorAttachment
      {
        .textureView = &gbufferColorView.value(),
        .clearValue = Fwog::ClearValue{.color{.f{ .1f, .3f, .5f, 0.0f } } },
        .clearOnLoad = true
      };
      Fwog::RenderAttachment gnormalAttachment
      {
        .textureView = &gbufferNormalView.value(),
        .clearValue = Fwog::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
        .clearOnLoad = false
      };
      Fwog::RenderAttachment gdepthAttachment
      {
        .textureView = &gbufferDepthView.value(),
        .clearValue = Fwog::ClearValue{.depthStencil{.depth = 0.0f } },
        .clearOnLoad = true
      };
      Fwog::RenderAttachment cgAttachments[] = { gcolorAttachment, gnormalAttachment };
      Fwog::RenderInfo gbufferRenderInfo
      {
        .viewport = &mainViewport,
        .colorAttachments = cgAttachments,
        .depthAttachment = &gdepthAttachment,
        .stencilAttachment = nullptr
      };
      Fwog::BeginRendering(gbufferRenderInfo);
      Fwog::ScopedDebugMarker marker("Geometry");
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      Fwog::Cmd::BindUniformBuffer(2, *materialUniformsBuffer, 0, materialUniformsBuffer->Size());

      Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer, 0, meshUniformBuffer->Size());
      for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
      {
        const auto& mesh = scene.meshes[i];
        const auto& material = scene.materials[mesh.materialIdx];
        materialUniformsBuffer->SubData(material.gpuMaterial, 0);
        if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
        {
          const auto& textureSampler = scene.textureSamplers[material.baseColorTextureIdx];
          Fwog::Cmd::BindSampledImage(0, *textureSampler.textureView, *textureSampler.sampler);
        }
        Fwog::Cmd::BindVertexBuffer(0, *mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
        Fwog::Cmd::BindIndexBuffer(*mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
        Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer->Size()) / sizeof(uint32_t), 1, 0, 0, i);
      }
      Fwog::EndRendering();
    }

    globalUniformsBuffer->SubData(shadingUniforms.sunViewProj, 0);

    // shadow map scene pass
    {
      Fwog::RenderAttachment depthAttachment
      {
        .textureView = &shadowDepthTexView.value(),
        .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
        .clearOnLoad = true
      };

      Fwog::RenderInfo shadowRenderInfo
      {
        .viewport = &shadowViewport,
        .depthAttachment = &depthAttachment,
        .stencilAttachment = nullptr
      };
      Fwog::BeginRendering(shadowRenderInfo);
      Fwog::ScopedDebugMarker marker("Shadow Scene");
      Fwog::Cmd::BindGraphicsPipeline(shadowPipeline);
      Fwog::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer, 0, meshUniformBuffer->Size());

      for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
      {
        const auto& mesh = scene.meshes[i];
        const auto& material = scene.materials[mesh.materialIdx];
        materialUniformsBuffer->SubData(material.gpuMaterial, 0);
        if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
        {
          const auto& textureSampler = scene.textureSamplers[material.baseColorTextureIdx];
          Fwog::Cmd::BindSampledImage(0, *textureSampler.textureView, *textureSampler.sampler);
        }
        Fwog::Cmd::BindVertexBuffer(0, *mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
        Fwog::Cmd::BindIndexBuffer(*mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
        Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer->Size()) / sizeof(uint32_t), 1, 0, 0, i);
      }
      Fwog::EndRendering();
    }

    globalUniformsBuffer->SubData(mainCameraUniforms, 0);

    // shading pass (full screen tri)
    {
      Fwog::RenderAttachment shadingAttachment
      {
        .textureView = &shadingTexView.value(),
        .clearOnLoad = false
      };

      Fwog::RenderInfo shadingRenderingInfo
      {
        .viewport = &mainViewport,
        .colorAttachments = { &shadingAttachment, 1 }
      };
      Fwog::BeginRendering(shadingRenderingInfo);
      Fwog::ScopedDebugMarker marker("Shading");
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, *gbufferColorView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(1, *gbufferNormalView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(2, *gbufferDepthView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(3, *shadowDepthTexView, *shadowSampler);
      Fwog::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      Fwog::Cmd::BindUniformBuffer(1, *shadingUniformsBuffer, 0, shadingUniformsBuffer->Size());
      Fwog::Cmd::BindStorageBuffer(0, *lightBuffer, 0, lightBuffer->Size());
      Fwog::Cmd::Draw(3, 1, 0, 0);
      Fwog::EndRendering();
    }

    // copy to esm and blur
    {
      Fwog::BeginCompute();
      {
        {
          Fwog::ScopedDebugMarker marker("Copy to ESM");
          esmUniformBuffer->SubData(config.esmExponent, 0);

          auto nearestSampler = Fwog::TextureSampler::Create({ .minFilter = Fwog::Filter::NEAREST, .magFilter = Fwog::Filter::NEAREST });
          Fwog::Cmd::BindComputePipeline(copyToEsmPipeline);
          Fwog::Cmd::BindSampledImage(0, *shadowDepthTexView, *nearestSampler);
          Fwog::Cmd::BindImage(0, *esmTexView, 0);
          Fwog::Cmd::BindUniformBuffer(0, *esmUniformBuffer, 0, esmUniformBuffer->Size());
          auto dispatchDim = (esmTexView->Extent() + 7) / 8;
          Fwog::Cmd::Dispatch(dispatchDim.width, dispatchDim.height, 1);

          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        }

        // blur
        {
          Fwog::ScopedDebugMarker marker("Blur ESM");
          Fwog::Cmd::BindComputePipeline(gaussianBlurPipeline);

          auto linearSampler = Fwog::TextureSampler::Create({ .minFilter = Fwog::Filter::LINEAR, .magFilter = Fwog::Filter::LINEAR });

          struct
          {
            glm::ivec2 direction;
            glm::ivec2 targetDim;
          }esmBlurUniforms;

          const auto esmExtent1 = esmTexView->Extent();
          const auto esmExtent2 = esmIntermediateTexView->Extent();

          const auto dispatchSize1 = (esmExtent2 + 7) / 8;
          const auto dispatchSize2 = (esmExtent1 + 7) / 8;

          Fwog::Cmd::BindUniformBuffer(0, *esmBlurUniformBuffer, 0, esmBlurUniformBuffer->Size());

          for (size_t i = 0; i < config.esmBlurPasses; i++)
          {
            esmBlurUniforms.direction = { 0, 1 };
            esmBlurUniforms.targetDim = { esmExtent2.width, esmExtent2.height };
            esmBlurUniformBuffer->SubData(esmBlurUniforms, 0);
            Fwog::Cmd::BindSampledImage(0, *esmTexView, *linearSampler);
            Fwog::Cmd::BindImage(0, *esmIntermediateTexView, 0);
            Fwog::Cmd::Dispatch(dispatchSize1.width, dispatchSize1.height, 1);

            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

            esmBlurUniforms.direction = { 1, 0 };
            esmBlurUniforms.targetDim = { esmExtent1.width, esmExtent1.height };
            esmBlurUniformBuffer->SubData(esmBlurUniforms, 0);
            Fwog::Cmd::BindSampledImage(0, *esmIntermediateTexView, *linearSampler);
            Fwog::Cmd::BindImage(0, *esmTexView, 0);
            Fwog::Cmd::Dispatch(dispatchSize2.width, dispatchSize2.height, 1);

            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          }
        }
      }
      Fwog::EndCompute();
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
        1.0f,
        60.0f,
        curFrame);

      volumetric.AccumulateDensity(*densityVolumeView);

      volumetric.MarchVolume(*densityVolumeView, *scatteringVolumeView, *esmTexView, *esmUniformBuffer, *lightBuffer);

      volumetric.ApplyDeferred(*shadingTexView,
        *gbufferDepthView,
        *shadingTexView,
        *scatteringVolumeView,
        *noiseTextureView);
    }

    // on my driver (Nvidia 3.25.1.27), blitting appears to not perform automatic linear->sRGB conversions
    // hence, a full-screen triangle will instead be used
    //Fwog::BlitTextureToSwapchain(*shadingTexView,
    //  {},
    //  {},
    //  shadingTexView->Extent(),
    //  shadingTexView->Extent(),
    //  Fwog::Filter::LINEAR);

    // copy to swapchain
    {
      Fwog::SwapchainRenderInfo swapchainRenderingInfo
      {
        .viewport = &mainViewport,
      };
      Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
      Fwog::ScopedDebugMarker marker("Copy to Swapchain");

      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      Fwog::TextureView* tex = &shadingTexView.value();
      if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        tex = &gbufferColorView.value();
      if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        tex = &gbufferNormalView.value();
      if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        tex = &gbufferDepthView.value();
      if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        tex = &shadowDepthTexView.value();
      if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS)
        tex = &esmTexView.value();
      if (tex)
      {
        Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
        Fwog::Cmd::BindSampledImage(0, *tex, *nearestSampler);
        Fwog::Cmd::Draw(3, 1, 0, 0);
      }
      Fwog::EndRendering();
    }

    glfwSwapBuffers(window);
  }

  glfwTerminate();
}

int main(int argc, const char* const* argv)
{
  try
  {
    std::optional<std::string_view> fileName;
    float scale = 1.0f;
    bool binary = false;

    if (argc > 1)
    {
      fileName = argv[1];
    }
    if (argc > 2)
    {
      auto [ptr, ec] = std::from_chars(argv[2], argv[2] + strlen(argv[2]), scale);
      if (ec != std::errc{})
      {
        throw std::exception("Scale should be a real number");
      }
    }
    if (argc > 3)
    {
      int val = 0;
      auto [ptr, ec] = std::from_chars(argv[3], argv[3] + strlen(argv[3]), val);
      binary = static_cast<bool>(val);
      if (ec != std::errc{})
      {
        throw std::exception("Binary should be 0 or 1");
      }
    }

    RenderScene(fileName, scale, binary);
  }
  catch (std::exception e)
  {
    printf("Error: %s\n", e.what());
    throw;
  }
  catch (...)
  {
    printf("Unknown error\n");
    throw;
  }

  return 0;
}