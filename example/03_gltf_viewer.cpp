#include "common/Application.h"
#include "common/RsmTechnique.h"
#include "common/SceneLoader.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/DebugMarker.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#include <stb_image.h>

#include <imgui.h>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <charconv>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

/* 03_gltf_viewer
 *
 * A simple model viewer for glTF scene files. This example build upon 02_deferred, which implements deferred rendering
 * and reflective shadow maps (RSM). Also implemented in this example are point lights.
 * 
 * The app has three optional arguments that must appear in order.
 * If a later option is used, the previous options must be use used as well.
 *
 * Options
 * Filename (string) : name of the glTF file you wish to view.
 * Scale (real)      : uniform scale factor in case the model is tiny or huge. Default: 1.0
 * Binary (int)      : whether the input file is binary glTF. Default: false
 *
 * If no options are specified, the default scene will be loaded.
 * 
 * TODO: add clustered light culling
 */

////////////////////////////////////// Types

struct ObjectUniforms
{
  glm::mat4 model;
};

struct GlobalUniforms
{
  glm::mat4 viewProj;
  glm::mat4 invViewProj;
  glm::mat4 proj;
  glm::vec4 cameraPos;
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
};

struct alignas(16) Light
{
  glm::vec4 position;
  glm::vec3 intensity;
  float invRadius;
  // uint32_t type; // 0 = point, 1 = spot
};

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

static Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferredPbr.vert.glsl"));
  auto fs =
    Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/SceneDeferredPbr.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true},
  });
}

static Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferredPbr.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/RSMScenePbr.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
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

static Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs =
    Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/ShadeDeferredPbr.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
  });
}

static Fwog::GraphicsPipeline CreateDebugTexturePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/Texture.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
  });
}

class GltfViewerApplication final : public Application
{
public:
  GltfViewerApplication(const Application::CreateInfo& createInfo,
                        std::optional<std::string_view> filename,
                        float scale,
                        bool binary);

private:
  void OnWindowResize(uint32_t newWidth, uint32_t newHeight) override;
  void OnUpdate(double dt) override;
  void OnRender(double dt) override;
  void OnGui(double dt) override;

  // constants
  static constexpr int gShadowmapWidth = 1024;
  static constexpr int gShadowmapHeight = 1024;

  double illuminationTime = 0;

  // scene parameters
  float sunPosition = -1.127f;

  // Resources tied to the swapchain/output size
  struct Frame
  {
    // g-buffer textures
    std::optional<Fwog::Texture> gAlbedo;
    std::optional<Fwog::Texture> gNormal;
    std::optional<Fwog::Texture> gDepth;
    std::optional<Fwog::Texture> gNormalPrev;
    std::optional<Fwog::Texture> gDepthPrev;
    std::optional<RSM::RsmTechnique> rsm;
  };
  Frame frame{};

  // Reflective shadow map textures
  Fwog::Texture rsmFlux;
  Fwog::Texture rsmNormal;
  Fwog::Texture rsmDepth;

  ShadingUniforms shadingUniforms;

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;
  Fwog::TypedBuffer<ShadingUniforms> shadingUniformsBuffer;
  Fwog::TypedBuffer<Utility::GpuMaterial> materialUniformsBuffer;

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline rsmScenePipeline;
  Fwog::GraphicsPipeline shadingPipeline;
  Fwog::GraphicsPipeline debugTexturePipeline;

  // Scene
  Utility::Scene scene;
  std::optional<Fwog::TypedBuffer<Light>> lightBuffer;
  std::optional<Fwog::TypedBuffer<ObjectUniforms>> meshUniformBuffer;
};

GltfViewerApplication::GltfViewerApplication(const Application::CreateInfo& createInfo,
                                             std::optional<std::string_view> filename,
                                             float scale,
                                             bool binary)
  : Application(createInfo),
    // Create RSM textures
    rsmFlux(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R11G11B10_FLOAT)),
    rsmNormal(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R16G16B16_SNORM)),
    rsmDepth(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::D16_UNORM)),
    // Create constant-size buffers
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    shadingUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    materialUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    // Create the pipelines used in the application
    scenePipeline(CreateScenePipeline()),
    rsmScenePipeline(CreateShadowPipeline()),
    shadingPipeline(CreateShadingPipeline()),
    debugTexturePipeline(CreateDebugTexturePipeline())
{
  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  cursorIsActive = true;

  cameraSpeed = 2.5f;
  mainCamera.position.y = 2;

  if (!filename)
  {
    Utility::LoadModelFromFile(scene, "models/simple_scene.glb", glm::mat4{.125}, true);
    // Utility::LoadModelFromFile(scene, "models/rock_terrain_3/scene.gltf", glm::mat4{.5}, false);
    // Utility::LoadModelFromFile(scene, "models/Sponza/glTF/Sponza.gltf", glm::mat4{.5}, false);
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

  //////////////////////////////////////// Clustered rendering stuff
  std::vector<Light> lights;
  // lights.push_back(Light{ .position = { 3, 2, 0, 0 }, .intensity = { .2f, .8f, 1.0f }, .invRadius = 1.0f / 4.0f });
  // lights.push_back(Light{ .position = { 3, -2, 0, 0 }, .intensity = { .7f, .8f, 0.1f }, .invRadius = 1.0f / 2.0f });
  // lights.push_back(Light{ .position = { 3, 2, 0, 0 }, .intensity = { 1.2f, .8f, .1f }, .invRadius = 1.0f / 6.0f });

  meshUniformBuffer.emplace(meshUniforms, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  lightBuffer.emplace(lights, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  // clusterTexture({.imageType = Fwog::ImageType::TEX_3D,
  //                                      .format = Fwog::Format::R16G16_UINT,
  //                                      .extent = {16, 9, 24},
  //                                      .mipLevels = 1,
  //                                      .arrayLayers = 1,
  //                                      .sampleCount = Fwog::SampleCount::SAMPLES_1},
  //                                     "Cluster Texture");

  //// atomic counter + uint array
  // clusterIndicesBuffer = Fwog::Buffer(sizeof(uint32_t) + sizeof(uint32_t) * 10000);
  // const uint32_t zero = 0; // what it says on the tin
  // clusterIndicesBuffer.ClearSubData(0,
  //                                   clusterIndicesBuffer.Size(),
  //                                   Fwog::Format::R32_UINT,
  //                                   Fwog::UploadFormat::R,
  //                                   Fwog::UploadType::UINT,
  //                                   &zero);

  OnWindowResize(windowWidth, windowHeight);
}

void GltfViewerApplication::OnWindowResize(uint32_t newWidth, uint32_t newHeight)
{
  // create gbuffer textures and render info
  frame.gAlbedo = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_UNORM);
  frame.gNormal = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepth = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_UNORM);
  frame.gNormalPrev = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepthPrev = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_UNORM);

  frame.rsm = RSM::RsmTechnique(newWidth, newHeight);
}

void GltfViewerApplication::OnUpdate([[maybe_unused]] double dt) {}

void GltfViewerApplication::OnRender([[maybe_unused]] double dt)
{
  std::swap(frame.gDepth, frame.gDepthPrev);
  std::swap(frame.gNormal, frame.gNormalPrev);

  shadingUniforms = ShadingUniforms{
    .sunDir = glm::normalize(glm::rotate(sunPosition, glm::vec3{1, 0, 0}) * glm::vec4{-.1, -.3, -.6, 0}),
    .sunStrength = glm::vec4{5, 5, 5, 0},
  };

  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  ss.compareEnable = true;
  ss.compareOp = Fwog::CompareOp::LESS;
  ss.minFilter = Fwog::Filter::LINEAR;
  ss.magFilter = Fwog::Filter::LINEAR;
  auto rsmShadowSampler = Fwog::Sampler(ss);

  auto proj = glm::perspective(glm::radians(70.f), windowWidth / (float)windowHeight, 0.1f, 100.f);

  GlobalUniforms mainCameraUniforms{};
  mainCameraUniforms.viewProj = proj * mainCamera.GetViewMatrix();
  mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
  mainCameraUniforms.cameraPos = glm::vec4(mainCamera.position, 0.0);

  globalUniformsBuffer.SubData(mainCameraUniforms, 0);

  glm::vec3 eye = glm::vec3{shadingUniforms.sunDir * -5.f};
  float eyeWidth = 7.0f;
  // shadingUniforms.viewPos = glm::vec4(camera.position, 0);
  shadingUniforms.sunViewProj = glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, -100.0f, 100.f) *
                                glm::lookAt(eye, glm::vec3(0), glm::vec3{0, 1, 0});
  shadingUniformsBuffer.SubData(shadingUniforms, 0);

  // Render scene geometry to the g-buffer
  {
    Fwog::RenderAttachment gAlbedoAttachment{.texture = &frame.gAlbedo.value(),
                                            .clearValue = Fwog::ClearColorValue{.1f, .3f, .5f, 0.0f},
                                            .clearOnLoad = true};
    Fwog::RenderAttachment gNormalAttachment{.texture = &frame.gNormal.value(),
                                             .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
                                             .clearOnLoad = false};
    Fwog::RenderAttachment gDepthAttachment{.texture = &frame.gDepth.value(),
                                            .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
                                            .clearOnLoad = true};
    Fwog::RenderAttachment cgAttachments[] = {gAlbedoAttachment, gNormalAttachment};
    Fwog::BeginRendering({
      .name = "Base Pass",
      .colorAttachments = cgAttachments,
      .depthAttachment = &gDepthAttachment,
      .stencilAttachment = nullptr,
    });
    Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer, 0, materialUniformsBuffer.Size());

    Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer, 0, meshUniformBuffer->Size());
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
  }
  Fwog::EndRendering();

  globalUniformsBuffer.SubData(shadingUniforms.sunViewProj, 0);

  // Shadow map (RSM) scene pass
  {
    Fwog::RenderAttachment rcolorAttachment{.texture = &rsmFlux,
                                            .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
                                            .clearOnLoad = false};
    Fwog::RenderAttachment rnormalAttachment{.texture = &rsmNormal,
                                             .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
                                             .clearOnLoad = false};
    Fwog::RenderAttachment rdepthAttachment{.texture = &rsmDepth,
                                            .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
                                            .clearOnLoad = true};
    Fwog::RenderAttachment crAttachments[] = {rcolorAttachment, rnormalAttachment};
    Fwog::BeginRendering({
      .name = "RSM Scene",
      .colorAttachments = crAttachments,
                                   .depthAttachment = &rdepthAttachment,
      .stencilAttachment = nullptr,
    });
    Fwog::Cmd::BindGraphicsPipeline(rsmScenePipeline);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer, 0, materialUniformsBuffer.Size());

    Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer, 0, meshUniformBuffer->Size());
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
  }
  Fwog::EndRendering();

  globalUniformsBuffer.SubData(mainCameraUniforms, 0);

  auto rsmCameraUniforms = RSM::CameraUniforms{
    .viewProj = mainCameraUniforms.viewProj,
    .invViewProj = mainCameraUniforms.invViewProj,
    .proj = proj,
    .cameraPos = glm::vec4(mainCamera.position, 0),
  };

  {
    static Fwog::TimerQueryAsync timer(5);
    if (auto t = timer.PopTimestamp())
    {
      illuminationTime = *t / 10e5;
    }
    Fwog::TimerScoped scopedTimer(timer);
  frame.rsm->ComputeIndirectLighting(shadingUniforms.sunViewProj,
                                     rsmCameraUniforms,
                                     frame.gAlbedo.value(),
                                     frame.gNormal.value(),
                                     frame.gDepth.value(),
                                     rsmFlux,
                                     rsmNormal,
                                     rsmDepth,
                                     frame.gDepthPrev.value(),
                                     frame.gNormalPrev.value());
  }

  // clear cluster indices atomic counter
  // clusterIndicesBuffer.ClearSubData(0, sizeof(uint32_t), Fwog::Format::R32_UINT, Fwog::UploadFormat::R, Fwog::UploadType::UINT, &zero);

  // record active clusters
  // TODO

  // light culling+cluster assignment

  //

  // shading pass (full screen tri)

  Fwog::BeginSwapchainRendering({
    .name = "Shading",
    .viewport =
      Fwog::Viewport{
        .drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}},
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      },
    .clearColorOnLoad = false,
    .clearColorValue = Fwog::ClearColorValue{.0f, .0f, .0f, 1.0f},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  });
  {
    Fwog::ScopedDebugMarker marker("Shading");
    Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
    Fwog::Cmd::BindSampledImage(0, *frame.gAlbedo, nearestSampler);
    Fwog::Cmd::BindSampledImage(1, *frame.gNormal, nearestSampler);
    Fwog::Cmd::BindSampledImage(2, *frame.gDepth, nearestSampler);
    Fwog::Cmd::BindSampledImage(3, frame.rsm->GetIndirectLighting(), nearestSampler);
    Fwog::Cmd::BindSampledImage(4, rsmDepth, rsmShadowSampler);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(0, *lightBuffer, 0, lightBuffer->Size());
    Fwog::Cmd::Draw(3, 1, 0, 0);

    const Fwog::Texture* tex{};
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
      tex = &frame.gAlbedo.value();
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
      tex = &frame.gNormal.value();
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
      tex = &frame.gDepth.value();
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
      tex = &frame.rsm->GetIndirectLighting();
    if (tex)
    {
      Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
      Fwog::Cmd::BindSampledImage(0, *tex, nearestSampler);
      Fwog::Cmd::Draw(3, 1, 0, 0);
    }
  }
  Fwog::EndRendering();
}

void GltfViewerApplication::OnGui([[maybe_unused]] double dt)
{
  ImGui::Begin("glTF Viewer");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Indirect Illumination: %f ms", illuminationTime);

  ImGui::SliderFloat("Sun Angle", &sunPosition, -2.7f, 0.5f);

  ImGui::Separator();

  frame.rsm->DrawGui();

  ImGui::BeginTabBar("tabbed");
  if (ImGui::BeginTabItem("G-Buffers"))
  {
    float aspect = float(windowWidth) / windowHeight;
    glTextureParameteri(frame.gAlbedo.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gAlbedo.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    glTextureParameteri(frame.gNormal.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gNormal.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    glTextureParameteri(frame.gDepth.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gDepth.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    glTextureParameteri(frame.rsm->GetIndirectLighting().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.rsm->GetIndirectLighting().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("RSM Buffers"))
  {
    glTextureParameteri(rsmDepth.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmDepth.Handle())), {100, 100}, {0, 1}, {1, 0});
    ImGui::SameLine();
    glTextureParameteri(rsmNormal.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmNormal.Handle())), {100, 100}, {0, 1}, {1, 0});
    ImGui::SameLine();
    glTextureParameteri(rsmFlux.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmFlux.Handle())), {100, 100}, {0, 1}, {1, 0});
    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
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

  auto appInfo = Application::CreateInfo{.name = "glTF Viewer Example"};
  auto app = GltfViewerApplication(appInfo, filename, scale, binary);
  app.Run();

  return 0;
}