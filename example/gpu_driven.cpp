/* volumetric.cpp
 *
 * Volumetric fog viewer.
 *
 * Takes the same command line arguments as the gltf_viewer example.
 */

#include "common/common.h"

#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <iostream>
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

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>

////////////////////////////////////// Externals
namespace ImGui
{
  ImGuiKeyData* GetKeyData(ImGuiKey key);
}

////////////////////////////////////// Types
struct View
{
  glm::vec3 position{};
  float pitch{}; // pitch angle in radians
  float yaw{};   // yaw angle in radians

  glm::vec3 GetForwardDir() const { return glm::vec3{cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw)}; }

  glm::mat4 GetViewMatrix() const { return glm::lookAt(position, position + GetForwardDir(), glm::vec3(0, 1, 0)); }
};

struct alignas(16) ObjectUniforms
{
  glm::mat4 model;
  uint32_t materialIdx;
};

struct alignas(16) BoundingBox
{
  glm::vec3 offset;
  uint32_t padding_0;
  glm::vec3 halfExtent;
  uint32_t padding_1;
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
  float viewNearPlane = 0.3f;
  bool freezeCulling = false;
  bool viewBoundingBoxes = false;
} config;

std::string LoadFileWithInclude(std::string_view path, std::string_view includeDir)
{
  char error[256] = {};
  char* included = stb_include_string(Utility::LoadFile(path).data(), nullptr, includeDir.data(), "", error);
  std::string includedStr = included;
  free(included);
  return includedStr;
}

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
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/SceneForward.vert.glsl", "shaders/gpu_driven"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/SceneForward.frag.glsl", "shaders/gpu_driven"));

  auto pipeline = Fwog::CompileGraphicsPipeline(
      {.name = "Generic material",
       .vertexShader = &vertexShader,
       .fragmentShader = &fragmentShader,
       .vertexInputState = {GetSceneInputBindingDescs()},
       .depthState = {.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::LESS}});

  return pipeline;
}

Fwog::GraphicsPipeline CreateBoundingBoxDebugPipeline()
{
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/BoundingBox.vert.glsl", "shaders/gpu_driven"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/SolidColor.frag.glsl", "shaders/gpu_driven"));

  auto pipeline = Fwog::CompileGraphicsPipeline({
      .name = "Wireframe bounding boxes",
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_STRIP},
      .rasterizationState = {.polygonMode = Fwog::PolygonMode::LINE, .cullMode = Fwog::CullMode::NONE},
      .depthState = {.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = Fwog::CompareOp::LESS},
  });

  return pipeline;
}

Fwog::GraphicsPipeline CreateBoundingBoxCullingPipeline()
{
  auto vertexShader =
      Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/BoundingBox.vert.glsl", "shaders/gpu_driven"));
  auto fragmentShader =
      Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                   LoadFileWithInclude("shaders/gpu_driven/CullVisibility.frag.glsl", "shaders/gpu_driven"));

  auto pipeline = Fwog::CompileGraphicsPipeline({
      .name = "Culling bounding boxes",
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_STRIP},
      .rasterizationState = {.polygonMode = Fwog::PolygonMode::FILL, .cullMode = Fwog::CullMode::NONE},
      .depthState = {.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = Fwog::CompareOp::LESS},
  });

  return pipeline;
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

void RenderScene(std::optional<std::string_view> fileName, float scale, bool binary)
{
  GLFWwindow* window = Utility::CreateWindow({.name = "GPU-Driven Rendering Example",
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
  glEnable(GL_FRAMEBUFFER_SRGB);

  Fwog::Viewport mainViewport{.drawRect{.extent = {gWindowWidth, gWindowHeight}}};

  // create gbuffer textures and render info
  auto gBufferColorTexture = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::R8G8B8A8_SRGB);
  auto gBufferDepthTexture = Fwog::CreateTexture2D({gWindowWidth, gWindowHeight}, Fwog::Format::D32_FLOAT);

  Fwog::GraphicsPipeline scenePipeline = CreateScenePipeline();
  Fwog::GraphicsPipeline boundingBoxDebugPipeline = CreateBoundingBoxDebugPipeline();
  Fwog::GraphicsPipeline boundingBoxCullingPipeline = CreateBoundingBoxCullingPipeline();

  Utility::SceneBindless scene;
  bool success = false;

  if (!fileName)
  {
    success = Utility::LoadModelFromFileBindless(scene, "models/simple_scene.glb", glm::mat4{.5}, true);
    // success = Utility::LoadModelFromFileBindless(scene, "models/BoomBox/glTF/BoomBox.gltf",
    // glm::mat4{ 100. });
  }
  else
  {
    // success = Utility::LoadModelFromFileBindless(scene, "models/simple_scene.glb", glm::mat4{ .5
    // }, true);
    success = Utility::LoadModelFromFileBindless(scene, *fileName, glm::scale(glm::vec3{scale}), binary);
  }

  // oof
  if (!success)
  {
    throw std::runtime_error("Failed to load");
  }

  std::vector<ObjectUniforms> meshUniforms;
  std::vector<BoundingBox> boundingBoxes;
  std::vector<uint32_t> objectIndices = {static_cast<uint32_t>(scene.meshes.size())};
  std::vector<Fwog::DrawIndexedIndirectCommand> drawCommands;

  int curObjectIndex = 0;
  for (const auto& mesh : scene.meshes)
  {
    meshUniforms.push_back(ObjectUniforms{.model = mesh.transform, .materialIdx = mesh.materialIdx});
    boundingBoxes.push_back(BoundingBox{
        .offset = mesh.boundingBox.offset,
        .halfExtent = mesh.boundingBox.halfExtent,
    });
    drawCommands.push_back(Fwog::DrawIndexedIndirectCommand{.indexCount = mesh.indexCount,
                                                            .instanceCount = 0,
                                                            .firstIndex = mesh.startIndex,
                                                            .vertexOffset = mesh.startVertex,
                                                            .firstInstance = 0});
    objectIndices.push_back(curObjectIndex++);
  }

  auto drawCommandsBuffer = Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>(drawCommands);
  auto vertexBuffer = Fwog::TypedBuffer<Utility::Vertex>(scene.vertices);
  auto indexBuffer = Fwog::TypedBuffer<Utility::index_t>(scene.indices);
  auto globalUniformsBuffer = Fwog::TypedBuffer<GlobalUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  auto meshUniformBuffer = Fwog::TypedBuffer<ObjectUniforms>(meshUniforms);
  auto boundingBoxesBuffer = Fwog::TypedBuffer<BoundingBox>(boundingBoxes);
  auto objectIndicesBuffer = Fwog::Buffer(std::span(objectIndices));
  auto materialsBuffer = Fwog::TypedBuffer<Utility::GpuMaterialBindless>(scene.materials);

  View camera;
  camera.position = {0, 1.5, 2};
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
    if (ImGui::GetKeyData(static_cast<ImGuiKey>(GLFW_KEY_GRAVE_ACCENT))->DownDuration == 0.0f)
    {
      cursorIsActive = !cursorIsActive;
      glfwSetInputMode(window, GLFW_CURSOR, cursorIsActive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    // hack to prevent the "disabled" (but actually just invisible) cursor from being able to click
    // stuff in ImGui
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
    ImGui::Checkbox("Freeze culling", &config.freezeCulling);
    ImGui::Checkbox("View bounding boxes", &config.viewBoundingBoxes);
    ImGui::End();

    if (!cursorIsActive)
    {
      camera = ProcessMovement(window, camera, dt);
    }

    // Update global uniforms
    GlobalUniforms mainCameraUniforms{};
    mainCameraUniforms.viewProj = proj * camera.GetViewMatrix();
    mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
    mainCameraUniforms.cameraPos = glm::vec4(camera.position, 0.0);
    globalUniformsBuffer.SubDataTyped(mainCameraUniforms);

    Fwog::RenderAttachment gDepthAttachment{.texture = &gBufferDepthTexture,
                                            .clearValue = Fwog::ClearValue{.depthStencil{.depth = 1.0f}},
                                            .clearOnLoad = true};

    // scene pass
    {
      Fwog::RenderAttachment gColorAttachment{.texture = &gBufferColorTexture,
                                              .clearValue = Fwog::ClearValue{.color{.f{.1f, .3f, .5f, 0.0f}}},
                                              .clearOnLoad = true};
      Fwog::BeginRendering({.name = "Scene",
                            .viewport = &mainViewport,
                            .colorAttachments = std::span(&gColorAttachment, 1),
                            .depthAttachment = &gDepthAttachment,
                            .stencilAttachment = nullptr});

      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::COMMAND_BUFFER_BIT |
                               Fwog::MemoryBarrierAccessBit::SHADER_STORAGE_BIT);

      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(0, meshUniformBuffer, 0, meshUniformBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(1, materialsBuffer, 0, materialsBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(2, boundingBoxesBuffer, 0, boundingBoxesBuffer.Size());
      Fwog::Cmd::BindStorageBuffer(3, objectIndicesBuffer, 0, objectIndicesBuffer.Size());

      // Draw the visible scene.
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindVertexBuffer(0, vertexBuffer, 0, sizeof(Utility::Vertex));
      Fwog::Cmd::BindIndexBuffer(indexBuffer, Fwog::IndexType::UNSIGNED_INT);
      Fwog::Cmd::DrawIndexedIndirect(drawCommandsBuffer, 0, static_cast<uint32_t>(scene.meshes.size()), 0);

      if (config.viewBoundingBoxes)
      {
        Fwog::Cmd::BindGraphicsPipeline(boundingBoxDebugPipeline);
        Fwog::Cmd::Draw(24, static_cast<uint32_t>(scene.meshes.size()), 0, 0);
      }

      Fwog::EndRendering();
    }

    if (!config.freezeCulling)
    {
      gDepthAttachment.clearOnLoad = false;
      Fwog::BeginRendering(
          {.name = "Occlusion culling", .viewport = &mainViewport, .depthAttachment = &gDepthAttachment});

      // Re-upload the draw commands buffer to reset the instance counts to 0 for culling
      drawCommandsBuffer = Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>(drawCommands);

      Fwog::Cmd::BindStorageBuffer(4, drawCommandsBuffer, 0, drawCommandsBuffer.Size());

      // Draw visible bounding boxes.
      Fwog::Cmd::BindGraphicsPipeline(boundingBoxCullingPipeline);
      Fwog::Cmd::Draw(24,
                      static_cast<uint32_t>(scene.meshes.size()),
                      0,
                      0); // TODO: upgrade to indirect draw after frustum culling is added

      Fwog::EndRendering();
    }

    Fwog::BlitTextureToSwapchain(gBufferColorTexture,
                                 {},
                                 {},
                                 {gWindowWidth, gWindowHeight},
                                 {gWindowWidth, gWindowHeight},
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

  // try
  //{
  RenderScene(fileName, scale, binary);
  //}
  // catch (std::exception& e)
  //{
  //  std::cout << "Runtime error!\n" << e.what() << '\n';
  //  return -1;
  //}

  return 0;
}