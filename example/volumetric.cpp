/* gltf_viewer.cpp
*
* A viewer of glTF files.
*
* The app has three optional arguments that must appear in order.
* If a later option is used, the previous options must be use used as well.
*
* Options
* Filename (string) : name of the glTF file you wish to view.
* Scale (real)      : uniform scale factor in case the model is tiny or huge. Default: 1.0
* Binary (int)      : whether the input file is binary glTF. Default: false
*
* If no options are specified, the default scene "models/hierarchyTest.glb" will be loaded.
*/

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

constexpr int gShadowmapWidth = 1024;
constexpr int gShadowmapHeight = 1024;

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
      }
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

  camera.yaw += gCursorOffsetX * gSensitivity;
  camera.pitch += gCursorOffsetY * gSensitivity;
  camera.pitch = glm::clamp(camera.pitch, -glm::half_pi<float>() + 1e-4f, glm::half_pi<float>() - 1e-4f);

  return camera;
}

namespace Volumetric
{
  struct CommonConfig
  {
    const View* view      = nullptr;
    const glm::mat4* proj = nullptr;
    float volumeNearPlane = 0;
    float volumeFarPlane  = 0;
  };

  void AccumulateDensity(
    const CommonConfig& common, 
    const Fwog::TextureView density);
  void MarchVolume(
    const CommonConfig& common, 
    const Fwog::TextureView& sourceVolume, 
    const Fwog::TextureView& targetVolume);
  void ApplyDeferred(
    const CommonConfig& common,
    const Fwog::TextureView& gbufferColor,
    const Fwog::TextureView& gbufferDepth,
    const Fwog::TextureView& targetColor,
    const Fwog::TextureView& sourceVolume,
    const Fwog::TextureView& noise);
}

void RenderScene(std::optional<std::string_view> fileName, float scale, bool binary)
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "glTF Viewer Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glEnable(GL_FRAMEBUFFER_SRGB);

  Fwog::Viewport mainViewport { .drawRect {.extent = { gWindowWidth, gWindowHeight } } };

  Fwog::Viewport shadowViewport { .drawRect { .extent = { gShadowmapWidth, gShadowmapHeight } } };

  Fwog::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = &mainViewport,
    .clearColorOnLoad = false,
    .clearColorValue = Fwog::ClearColorValue {.f = { .0, .0, .0, 1.0 }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  // create gbuffer textures and render info
  auto gcolorTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R8G8B8A8_UNORM);
  auto gnormalTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R16G16B16_SNORM);
  auto gdepthTex = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::D32_UNORM);
  auto gbufferColorView = gcolorTex->View();
  auto gbufferNormalView = gnormalTex->View();
  auto gbufferDepthView = gdepthTex->View();
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
    .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
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

  // create shadow depth texture and render info
  auto shadowDepthTex = Fwog::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, Fwog::Format::D16_UNORM);
  auto shadowDepthTexView = shadowDepthTex->View();

  Fwog::RenderAttachment rdepthAttachment
  {
    .textureView = &shadowDepthTexView.value(),
    .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };

  Fwog::RenderInfo shadowRenderInfo
  {
    .viewport = &shadowViewport,
    .depthAttachment = &rdepthAttachment,
    .stencilAttachment = nullptr
  };

  auto proj = glm::perspective(glm::radians(70.f), gWindowWidth / (float)gWindowHeight, 0.1f, 100.f);

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
  lights.push_back(Light{ .position = { 3, 2, 0, 0 }, .intensity = { .2f, .8f, 1.0f }, .invRadius = 1.0f / 4.0f });
  lights.push_back(Light{ .position = { 3, -2, 0, 0 }, .intensity = { .7f, .8f, 0.1f }, .invRadius = 1.0f / 2.0f });
  lights.push_back(Light{ .position = { 3, 2, 0, 0 }, .intensity = { 1.2f, .8f, .1f }, .invRadius = 1.0f / 6.0f });

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
  ss.compareOp = Fwog::CompareOp::LESS;
  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  auto shadowSampler = Fwog::TextureSampler::Create(ss);

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();
  Fwog::GraphicsPipeline shadowPipeline = CreateShadowPipeline();
  Fwog::GraphicsPipeline shadingPipeline = CreateShadingPipeline();
  Fwog::GraphicsPipeline debugTexturePipeline = CreateDebugTexturePipeline();

  View camera;
  camera.position = { 0, 1.5, 2 };
  camera.yaw = -glm::half_pi<float>();

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
      glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, .1f, 20.f) *
      glm::lookAt(eye, glm::vec3(0), glm::vec3{ 0, 1, 0 });
    shadingUniformsBuffer->SubData(shadingUniforms, 0);

    // geometry buffer pass
    Fwog::BeginRendering(gbufferRenderInfo);
    {
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
    }
    Fwog::EndRendering();

    globalUniformsBuffer->SubData(shadingUniforms.sunViewProj, 0);

    // shadow map scene pass
    Fwog::BeginRendering(shadowRenderInfo);
    {
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
    }
    Fwog::EndRendering();

    globalUniformsBuffer->SubData(mainCameraUniforms, 0);

    // shading pass (full screen tri)
    Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
    {
      Fwog::ScopedDebugMarker marker("Shading");
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, *gbufferColorView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(1, *gbufferNormalView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(2, *gbufferDepthView, *nearestSampler);
      Fwog::Cmd::BindSampledImage(3, *shadowDepthTexView, *shadowSampler);
      Fwog::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      Fwog::Cmd::BindUniformBuffer(1, *shadingUniformsBuffer, 0, shadingUniformsBuffer->Size());
      Fwog::Cmd::BindStorageBuffer(0, *lightBuffer, 0, lightBuffer->Size());
      Fwog::Cmd::Draw(3, 1, 0, 0);

      Fwog::TextureView* tex{};
      if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        tex = &gbufferColorView.value();
      if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        tex = &gbufferNormalView.value();
      if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        tex = &gbufferDepthView.value();
      if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        tex = &shadowDepthTexView.value();
      if (tex)
      {
        Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
        Fwog::Cmd::BindSampledImage(0, *tex, *nearestSampler);
        Fwog::Cmd::Draw(3, 1, 0, 0);
      }
    }
    Fwog::EndRendering();

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