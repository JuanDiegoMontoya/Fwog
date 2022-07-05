/* volumetric.cpp
*
* Volumetric fog viewer.
*
* Takes the same command line arguments as the gltf_viewer example.
*/

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "common/common.h"

#include <array>
#include <vector>
#include <string>
#include <charconv>
#include <exception>
#include <fstream>

#include <Fwog/BasicTypes.h>
#include <Fwog/Rendering.h>
#include <Fwog/Pipeline.h>
#include <Fwog/DebugMarker.h>
#include <Fwog/Timer.h>
#include <Fwog/Texture.h>
#include <Fwog/Buffer.h>
#include <Fwog/Shader.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>

#include "common/SceneLoader.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

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

struct alignas(16) ObjectUniforms
{
  glm::mat4 model;
  uint32_t materialIdx;
  uint32_t padding[3];
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
  Fwog::Extent3D shadowmapResolution = { 2048, 2048 };

  float viewNearPlane = 0.3f;

  float lightFarPlane = 50.0f;
  float lightProjWidth = 24.0f;
  float lightDistance = 25.0f;
}config;

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
  auto vertexShader = Fwog::Shader(
    Fwog::PipelineStage::VERTEX_SHADER,
    Utility::LoadFile("shaders/SceneForward.vert.glsl"));
  auto fragmentShader = Fwog::Shader(
    Fwog::PipelineStage::FRAGMENT_SHADER,
    Utility::LoadFile("shaders/SceneForward.frag.glsl"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .vertexInputState = GetSceneInputBindingDescs(),
      .depthState = {.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::LESS }
    });

  return pipeline;
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

void RenderScene(std::optional<std::string_view> fileName, float scale, bool binary)
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "GPU-Driven Rendering Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsDark();
  auto* font = ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glEnable(GL_FRAMEBUFFER_SRGB);

  Fwog::Viewport mainViewport{ .drawRect {.extent = { gWindowWidth, gWindowHeight } } };

  Fwog::Viewport shadowViewport{ .drawRect {.extent = config.shadowmapResolution } };

  // create gbuffer textures and render info
  auto gBufferColorTexture = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::R8G8B8A8_SRGB);
  auto gBufferDepthTexture = Fwog::CreateTexture2D({ gWindowWidth, gWindowHeight }, Fwog::Format::D32_FLOAT);

  // create shadow depth texture and render info
  auto shadowDepthTexture = Fwog::CreateTexture2D(config.shadowmapResolution, Fwog::Format::D16_UNORM);

  Utility::SceneBindless scene;
  bool success = false;

  if (!fileName)
  {
    success = Utility::LoadModelFromFileBindless(scene, "models/simple_scene.glb", glm::mat4{ .5 }, true);
    //success = Utility::LoadModelFromFileBindless(scene, "models/BoomBox/glTF/BoomBox.gltf", glm::mat4{ 100. });
  }
  else
  {
    success = Utility::LoadModelFromFileBindless(scene, *fileName, glm::scale(glm::vec3{ scale }), binary);
  }

  // oof
  if (!success)
  {
    throw std::exception("Failed to load");
  }

  std::vector<ObjectUniforms> meshUniforms;
  std::vector<Fwog::DrawIndexedIndirectCommand> drawCommands;

  for (const auto& mesh : scene.meshes)
  {
    meshUniforms.push_back({ mesh.transform, mesh.materialIdx });
    drawCommands.push_back(Fwog::DrawIndexedIndirectCommand
      {
        .indexCount = mesh.indexCount,
        .instanceCount = 1,
        .firstIndex = mesh.startIndex,
        .vertexOffset = mesh.startVertex,
        .firstInstance = 0
      });
  }

  auto vertexBuffer = Fwog::TypedBuffer<Utility::Vertex>(scene.vertices);
  auto indexBuffer = Fwog::TypedBuffer<Utility::index_t>(scene.indices);
  auto drawCommandsBuffer = Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>(drawCommands);
  auto globalUniformsBuffer = Fwog::TypedBuffer<GlobalUniforms>(Fwog::BufferFlag::DYNAMIC_STORAGE | Fwog::BufferFlag::MAP_WRITE);
  auto meshUniformBuffer = Fwog::TypedBuffer<ObjectUniforms>(meshUniforms);
  auto materialsBuffer = Fwog::TypedBuffer<Utility::GpuMaterialBindless>(scene.materials);

  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();

  View camera;
  camera.position = { 0, 1.5, 2 };
  camera.yaw = -glm::half_pi<float>();

  const auto fovy = glm::radians(70.f);
  const auto aspectRatio = gWindowWidth / (float)gWindowHeight;
  auto proj = glm::perspective(fovy, aspectRatio, 0.3f, 100.f);

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
    if (ImGui::GetIO().KeysDownDuration[GLFW_KEY_GRAVE_ACCENT] == 0.0f)
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
    ImGui::Text("Henlo");
    for (const auto& [texture, sampler] : scene.textureSamplers)
    {
      ImGui::Image(reinterpret_cast<ImTextureID>(texture.Handle()), { 100, 100 });
    }
    ImGui::End();

    if (!cursorIsActive)
    {
      camera = ProcessMovement(window, camera, dt);
    }

    // update global uniforms
    GlobalUniforms mainCameraUniforms{};
    mainCameraUniforms.viewProj = proj * camera.GetViewMatrix();
    mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
    mainCameraUniforms.cameraPos = glm::vec4(camera.position, 0.0);
    globalUniformsBuffer.SubDataTyped(mainCameraUniforms);

    // scene pass
    {
      Fwog::ScopedDebugMarker marker("Scene");
      Fwog::RenderAttachment gColorAttachment
      {
        .texture = &gBufferColorTexture,
        .clearValue = Fwog::ClearValue{.color{.f{ .1f, .3f, .5f, 0.0f } } },
        .clearOnLoad = true
      };
      Fwog::RenderAttachment gDepthAttachment
      {
        .texture = &gBufferDepthTexture,
        .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f } },
        .clearOnLoad = true
      };
      Fwog::BeginRendering(
        {
          .viewport = &mainViewport,
          .colorAttachments = { &gColorAttachment, 1 },
          .depthAttachment = &gDepthAttachment,
          .stencilAttachment = nullptr
        });

      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      //Fwog::Cmd::BindUniformBuffer(1, materialUniformsBuffer, 0, materialUniformsBuffer.Size());

      Fwog::Cmd::BindStorageBuffer(0, meshUniformBuffer, 0, meshUniformBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(1, materialsBuffer, 0, materialsBuffer.Size());

      Fwog::Cmd::BindVertexBuffer(0, vertexBuffer, 0, sizeof(Utility::Vertex));
      Fwog::Cmd::BindIndexBuffer(indexBuffer, Fwog::IndexType::UNSIGNED_INT);
      
      Fwog::Cmd::DrawIndexedIndirect(drawCommandsBuffer, 0, scene.meshes.size(), 0);

      //for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
      //{
      //  const auto& mesh = scene.meshes[i];
      //  //const auto& material = scene.materials[mesh.materialIdx];
      //  //materialUniformsBuffer.SubData(material.gpuMaterial, 0);
      //  //if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
      //  //{
      //  //  const auto& textureSampler = scene.textureSamplers[material.baseColorTextureIdx];
      //  //  Fwog::Cmd::BindSampledImage(0, textureSampler.texture, textureSampler.sampler);
      //  //}
      //  //Fwog::Cmd::BindVertexBuffer(0, mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
      //  //Fwog::Cmd::BindIndexBuffer(mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
      //  //Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer.Size()) / sizeof(uint32_t), 1, 0, 0, i);
      //  Fwog::Cmd::DrawIndexed(mesh.indexCount, 1, mesh.startIndex, mesh.startVertex, i);
      //}

      Fwog::EndRendering();
    }

    Fwog::BlitTextureToSwapchain(gBufferColorTexture, 
      {}, 
      {}, 
      { gWindowWidth, gWindowHeight }, 
      { gWindowWidth, gWindowHeight },
      Fwog::Filter::NEAREST);

    ImGui::Render();
    {
      auto marker = Fwog::ScopedDebugMarker("Draw GUI");
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  }
  catch (std::exception e)
  {
    printf("Argument parsing error: %s\n", e.what());
    return -1;
  }

  RenderScene(fileName, scale, binary);

  return 0;
}