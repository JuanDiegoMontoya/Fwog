#include "common/Application.h"

#include <array>
#include <optional>
#include <tuple>
#include <vector>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/DebugMarker.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

/* 02_deferred
 *
 * This example implements a deferred renderer to visualize a simple 3D box scene. Deferred rendering is a technique
 * which renders the scene in two main passes instead of one. The first pass draws material properties to multiple render
 * targets: normal, albedo, and depth. The second pass uses a full-screen shader and these material properties to shade 
 * the scene.
 * 
 * This example also implements the paper reflective shadow maps (RSM) by Carsten Dachsbacher and Marc Stamminger.
 * RSM is an extension of shadow maps which adds normals and radiant flux render targets to the shadow pass to form
 * an RSM. Then, the RSM can be treated as a grid of point lights which is sampled several times to approximate one bounce
 * of indirect illumination. Also implemented is an extension of RSM which improves sampling and adds an edge-stopping
 * a-trous filter to blur the shadows, producing higher quality and cheaper indirect illumination.
 * 
 * In the GUI, RSM properties can be modified and other information about the scene can be viewed.
 *
 * Shown (+ indicates new features):
 * - Creating vertex buffers
 * - Specifying vertex attributes
 * - Loading shaders
 * - Creating a graphics pipeline
 * - Rendering to the screen
 * + Creating and dispatching compute pipelines
 * + Rendering to multiple off-screen render targets
 * + Dynamic uniform and storage buffers
 * + Memory barriers
 * + Profiling with timer queries
 * + Texture loading
 * + Sampler creation
 * + Sampled textures and images
 *
 */

////////////////////////////////////// Types

struct ObjectUniforms
{
  glm::mat4 model;
  glm::vec4 color;
};

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
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

struct RSMUniforms
{
  glm::mat4 sunViewProj;
  glm::mat4 invSunViewProj;
  glm::ivec2 targetDim;
  float rMax;
  uint32_t currentPass;
  uint32_t samples;
};

////////////////////////////////////// Globals

std::array<Vertex, 24> gCubeVertices{
  // front (+z)
  Vertex{{-0.5, -0.5, 0.5}, {0, 0, 1}, {0, 0}},
  {{0.5, -0.5, 0.5}, {0, 0, 1}, {1, 0}},
  {{0.5, 0.5, 0.5}, {0, 0, 1}, {1, 1}},
  {{-0.5, 0.5, 0.5}, {0, 0, 1}, {0, 1}},

  // back (-z)
  {{-0.5, 0.5, -0.5}, {0, 0, -1}, {1, 1}},
  {{0.5, 0.5, -0.5}, {0, 0, -1}, {0, 1}},
  {{0.5, -0.5, -0.5}, {0, 0, -1}, {0, 0}},
  {{-0.5, -0.5, -0.5}, {0, 0, -1}, {1, 0}},

  // left (-x)
  {{-0.5, -0.5, -0.5}, {-1, 0, 0}, {0, 0}},
  {{-0.5, -0.5, 0.5}, {-1, 0, 0}, {1, 0}},
  {{-0.5, 0.5, 0.5}, {-1, 0, 0}, {1, 1}},
  {{-0.5, 0.5, -0.5}, {-1, 0, 0}, {0, 1}},

  // right (+x)
  {{0.5, 0.5, -0.5}, {1, 0, 0}, {1, 1}},
  {{0.5, 0.5, 0.5}, {1, 0, 0}, {0, 1}},
  {{0.5, -0.5, 0.5}, {1, 0, 0}, {0, 0}},
  {{0.5, -0.5, -0.5}, {1, 0, 0}, {1, 0}},

  // top (+y)
  {{-0.5, 0.5, 0.5}, {0, 1, 0}, {0, 0}},
  {{0.5, 0.5, 0.5}, {0, 1, 0}, {1, 0}},
  {{0.5, 0.5, -0.5}, {0, 1, 0}, {1, 1}},
  {{-0.5, 0.5, -0.5}, {0, 1, 0}, {0, 1}},

  // bottom (-y)
  {{-0.5, -0.5, -0.5}, {0, -1, 0}, {0, 0}},
  {{0.5, -0.5, -0.5}, {0, -1, 0}, {1, 0}},
  {{0.5, -0.5, 0.5}, {0, -1, 0}, {1, 1}},
  {{-0.5, -0.5, 0.5}, {0, -1, 0}, {0, 1}},
};

std::array<uint16_t, 36> gCubeIndices{
  0,  1,  2,  2,  3,  0,

  4,  5,  6,  6,  7,  4,

  8,  9,  10, 10, 11, 8,

  12, 13, 14, 14, 15, 12,

  16, 17, 18, 18, 19, 16,

  20, 21, 22, 22, 23, 20,
};

static constexpr auto sceneInputBindingDescs = std::array{
  Fwog::VertexInputBindingDescription{
    // color
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, position),
  },
  Fwog::VertexInputBindingDescription{
    // normal
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, normal),
  },
  Fwog::VertexInputBindingDescription{
    // texcoord
    .location = 2,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = offsetof(Vertex, uv),
  },
};

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferred.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/SceneDeferred.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true},
  });
}

Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferred.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/RSMScene.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .rasterizationState =
      {
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasSlopeFactor = 2.0f,
      },
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true},
  });
}

Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/ShadeDeferred.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .rasterizationState = {.cullMode = Fwog::CullMode::NONE},
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
  });
}

Fwog::ComputePipeline CreateRSMIndirectPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/RSMIndirect.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &cs});
}

Fwog::ComputePipeline CreateRSMIndirectDitheredFilteredPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER,
                         Application::LoadFile("shaders/RSMIndirectDitheredFiltered.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &cs});
}

class DeferredApplication final : public Application
{
public:
  DeferredApplication(const Application::CreateInfo& createInfo);

private:
  void OnWindowResize(uint32_t newWidth, uint32_t newHeight) override;
  void OnUpdate(double dt) override;
  void OnRender(double dt) override;
  void OnGui(double dt) override;

  // constants
  static constexpr int gShadowmapWidth = 1024;
  static constexpr int gShadowmapHeight = 1024;

  // scene parameters
  int rsmSamples = 400;
  int rsmFilteredSamples = 15;
  float gRMax = 0.08f;
  bool rsmFiltered = false;
  bool cursorIsActive = false;
  float sunPosition = 0;

  double illuminationTime = 0;

  uint32_t sceneInstanceCount = 0;

  std::optional<Fwog::Texture> blueNoise;

  // Resources tied to the swapchain/output size
  struct Frame
  {
    // g-buffer textures
    std::optional<Fwog::Texture> gcolorTex;
    std::optional<Fwog::Texture> gnormalTex;
    std::optional<Fwog::Texture> gdepthTex;
    std::optional<Fwog::Texture> indirectLightingTex;
    std::optional<Fwog::Texture> indirectLightingTexPingPong;
  };
  Frame frame{};

  // Buffers describing the scene's objects and geometry
  std::optional<Fwog::Buffer> vertexBuffer;
  std::optional<Fwog::Buffer> indexBuffer;
  std::optional<Fwog::Buffer> objectBuffer;

  // reflective shadow map textures
  Fwog::Texture rfluxTex;
  Fwog::Texture rnormalTex;
  Fwog::Texture rdepthTex;

  ShadingUniforms shadingUniforms;
  RSMUniforms rsmUniforms;

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;
  Fwog::TypedBuffer<ShadingUniforms> shadingUniformsBuffer;
  Fwog::TypedBuffer<RSMUniforms> rsmUniformBuffer;

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline rsmScenePipeline;
  Fwog::GraphicsPipeline shadingPipeline;
  Fwog::ComputePipeline rsmIndirectPipeline;
  Fwog::ComputePipeline rsmIndirectDitheredFilteredPipeline;
  Fwog::GraphicsPipeline debugTexturePipeline;
};

DeferredApplication::DeferredApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    // Create RSM textures
    rfluxTex(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R11G11B10_FLOAT)),
    rnormalTex(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R16G16B16_SNORM)),
    rdepthTex(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::D16_UNORM)),
    // Create constant-size buffers
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    shadingUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    rsmUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    // Create the pipelines used in the app
    scenePipeline(CreateScenePipeline()),
    rsmScenePipeline(CreateShadowPipeline()),
    shadingPipeline(CreateShadingPipeline()),
    rsmIndirectPipeline(CreateRSMIndirectPipeline()),
    rsmIndirectDitheredFilteredPipeline(CreateRSMIndirectDitheredFilteredPipeline()),
    debugTexturePipeline(CreateDebugTexturePipeline())
{
  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  cursorIsActive = false;

  cameraSpeed = 1.0f;

  // load blue noise texture
  int x = 0;
  int y = 0;
  auto noise = stbi_load("textures/bluenoise16.png", &x, &y, nullptr, 4);
  assert(noise);
  blueNoise.emplace(
    Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM));
  blueNoise->SubImage({
    .dimension = Fwog::UploadDimension::TWO,
    .level = 0,
    .offset = {},
    .size = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
    .format = Fwog::UploadFormat::RGBA,
    .type = Fwog::UploadType::UBYTE,
    .pixels = noise,
  });
  stbi_image_free(noise);

  std::vector<ObjectUniforms> objectUniforms;
  // translation, scale, color tuples
  std::tuple<glm::vec3, glm::vec3, glm::vec3> objects[]{
    {{0, .5, -1}, {3, 1, 1}, {.5, .5, .5}},
    {{-1, .5, 0}, {1, 1, 1}, {.1, .1, .9}},
    {{1, .5, 0}, {1, 1, 1}, {.1, .1, .9}},
    {{0, -.5, -.5}, {3, 1, 2}, {.5, .5, .5}},
    {{0, 1.5, -.5}, {3, 1, 2}, {.2, .7, .2}},
    {{0, .25, 0}, {0.25, .5, .25}, {.5, .1, .1}},
    //{ { -.25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { .25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { 0, .25, -.25 },   { .7, .5, .01 }, { .5, .1, .1 } },
    //{ { 0, .25, .25 },   { .7, .5, .01 }, { .5, .1, .1 } },
  };
  for (const auto& [translation, scale, color] : objects)
  {
    glm::mat4 model{1};
    model = glm::translate(model, translation);
    model = glm::scale(model, scale);
    objectUniforms.push_back({model, glm::vec4{color, 0.0f}});
  }
  sceneInstanceCount = static_cast<uint32_t>(objectUniforms.size());

  vertexBuffer.emplace(gCubeVertices);
  indexBuffer.emplace(gCubeIndices);
  objectBuffer.emplace(std::span(objectUniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

  mainCamera.position = {0, .5, 1};
  mainCamera.yaw = -glm::half_pi<float>();

  OnWindowResize(windowWidth, windowHeight);
}

void DeferredApplication::OnWindowResize(uint32_t newWidth, uint32_t newHeight)
{
  // create gbuffer textures and render info
  frame.gcolorTex.emplace(Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_UNORM));
  frame.gnormalTex.emplace(Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM));
  frame.gdepthTex.emplace(Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_UNORM));

  frame.indirectLightingTex.emplace(Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16A16_FLOAT));
  frame.indirectLightingTexPingPong.emplace(Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16A16_FLOAT));

  rsmUniforms = {
    .targetDim = {newWidth, newHeight},
    .rMax = gRMax,
    .samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples),
  };
}

void DeferredApplication::OnUpdate([[maybe_unused]] double dt) {}

void DeferredApplication::OnRender([[maybe_unused]] double dt)
{
  shadingUniforms = ShadingUniforms{
    .sunDir = glm::normalize(glm::rotate(sunPosition, glm::vec3{1, 0, 0}) * glm::vec4{-.1, -.3, -.6, 0}),
    .sunStrength = glm::vec4{2, 2, 2, 0},
  };

  auto proj = glm::perspective(glm::radians(70.f), windowWidth / (float)windowHeight, 0.1f, 100.f);
  glm::mat4 viewProj = proj * mainCamera.GetViewMatrix();
  globalUniformsBuffer.SubData(viewProj, 0);
  globalUniformsBuffer.SubData(proj, offsetof(GlobalUniforms, proj));

  glm::vec3 eye = glm::vec3{shadingUniforms.sunDir * -5.f};
  float eyeWidth = 2.5f;
  // shadingUniforms.viewPos = glm::vec4(camera.position, 0);
  shadingUniforms.sunViewProj =
    glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, .1f, 10.f) * glm::lookAt(eye, glm::vec3(0), glm::vec3{0, 1, 0});
  shadingUniformsBuffer.SubData(shadingUniforms, 0);

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

  // geometry buffer pass
  Fwog::RenderAttachment gcolorAttachment{
    .texture = &frame.gcolorTex.value(),
    .clearValue = Fwog::ClearColorValue{.1f, .3f, .5f, 0.0f},
    .clearOnLoad = true,
  };
  Fwog::RenderAttachment gnormalAttachment{
    .texture = &frame.gnormalTex.value(),
    .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
    .clearOnLoad = false,
  };
  Fwog::RenderAttachment gdepthAttachment{
    .texture = &frame.gdepthTex.value(),
    .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
    .clearOnLoad = true,
  };
  Fwog::RenderAttachment cgAttachments[] = {gcolorAttachment, gnormalAttachment};
  Fwog::RenderInfo gbufferRenderInfo{
    .colorAttachments = cgAttachments,
    .depthAttachment = &gdepthAttachment,
    .stencilAttachment = nullptr,
  };
  Fwog::BeginRendering(gbufferRenderInfo);
  {
    Fwog::ScopedDebugMarker marker("Geometry");
    Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
    Fwog::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
    Fwog::Cmd::BindIndexBuffer(*indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(1, *objectBuffer, 0, objectBuffer->Size());
    Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), sceneInstanceCount, 0, 0, 0);
  }
  Fwog::EndRendering();

  globalUniformsBuffer.SubData(shadingUniforms.sunViewProj, 0);

  // shadow map (RSM) scene pass
  Fwog::RenderAttachment rcolorAttachment{
    .texture = &rfluxTex,
    .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
    .clearOnLoad = false,
  };
  Fwog::RenderAttachment rnormalAttachment{
    .texture = &rnormalTex,
    .clearValue = Fwog::ClearColorValue{0.f, 0.f, 0.f, 0.f},
    .clearOnLoad = false,
  };
  Fwog::RenderAttachment rdepthAttachment{
    .texture = &rdepthTex,
    .clearValue = Fwog::ClearDepthStencilValue{.depth = 1.0f},
    .clearOnLoad = true,
  };
  Fwog::RenderAttachment crAttachments[] = {rcolorAttachment, rnormalAttachment};
  Fwog::RenderInfo rsmRenderInfo{
    .colorAttachments = crAttachments,
    .depthAttachment = &rdepthAttachment,
    .stencilAttachment = nullptr,
  };
  Fwog::BeginRendering(rsmRenderInfo);
  {
    Fwog::ScopedDebugMarker marker("RSM Scene");
    Fwog::Cmd::BindGraphicsPipeline(rsmScenePipeline);
    Fwog::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
    Fwog::Cmd::BindIndexBuffer(*indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(1, *objectBuffer, 0, objectBuffer->Size());
    Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), sceneInstanceCount, 0, 0, 0);
  }
  Fwog::EndRendering();

  globalUniformsBuffer.SubData(viewProj, 0);
  globalUniformsBuffer.SubData(glm::inverse(viewProj), sizeof(glm::mat4));

  rsmUniforms.sunViewProj = shadingUniforms.sunViewProj;
  rsmUniforms.invSunViewProj = glm::inverse(rsmUniforms.sunViewProj);
  rsmUniformBuffer.SubData(rsmUniforms, 0);

  // RSM indirect illumination calculation pass
  Fwog::BeginCompute();
  {
    static Fwog::TimerQueryAsync timer(5);
    if (auto t = timer.PopTimestamp())
    {
      illuminationTime = *t / 10e5;
      // printf("Indirect Illumination: %f ms\n", *t / 10e5);
    }
    Fwog::TimerScoped scopedTimer(timer);

    Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(1, *frame.gcolorTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(2, *frame.gnormalTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(3, *frame.gdepthTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(4, rfluxTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(5, rnormalTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(6, rdepthTex, nearestSampler);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, rsmUniformBuffer, 0, rsmUniformBuffer.Size());
    Fwog::Cmd::BindImage(0, *frame.indirectLightingTex, 0);

    Fwog::ScopedDebugMarker marker("Indirect Illumination");
    if (rsmFiltered)
    {
      Fwog::Cmd::BindComputePipeline(rsmIndirectDitheredFilteredPipeline);
      Fwog::Cmd::BindSampledImage(7, *blueNoise, nearestSampler);

      const int localSize = 8;
      const int numGroupsX = (rsmUniforms.targetDim.x + localSize - 1) / localSize;
      const int numGroupsY = (rsmUniforms.targetDim.y + localSize - 1) / localSize;

      uint32_t currentPass = 0;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT |
                               Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);

      {
        Fwog::ScopedDebugMarker marker2("Filter Subsampled");
        for (int i = 0; i < 2; ++i)
        {
          currentPass = 1;
          rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
          Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTex, nearestSampler);
          Fwog::Cmd::BindImage(0, *frame.indirectLightingTexPingPong, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
          currentPass = 2;
          rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
          Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTexPingPong, nearestSampler);
          Fwog::Cmd::BindImage(0, *frame.indirectLightingTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
        }
      }

      {
        Fwog::ScopedDebugMarker marker2("Filter Box");
        currentPass = 3;
        rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
        Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTex, nearestSampler);
        Fwog::Cmd::BindImage(0, *frame.indirectLightingTexPingPong, 0);
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
        currentPass = 4;
        rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
        Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTexPingPong, nearestSampler);
        Fwog::Cmd::BindImage(0, *frame.indirectLightingTex, 0);
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
      }

      {
        Fwog::ScopedDebugMarker marker2("Modulate Albedo");
        currentPass = 5;
        rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
        Fwog::Cmd::BindSampledImage(0, *frame.indirectLightingTex, nearestSampler);
        Fwog::Cmd::BindImage(0, *frame.indirectLightingTexPingPong, 0);
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
      }

      std::swap(frame.indirectLightingTex, frame.indirectLightingTexPingPong);
    }
    else // Unfiltered RSM: the original paper
    {
      Fwog::Cmd::BindComputePipeline(rsmIndirectPipeline);

      const int localSize = 8;
      const int numGroupsX = (rsmUniforms.targetDim.x / 2 + localSize - 1) / localSize;
      const int numGroupsY = (rsmUniforms.targetDim.y / 2 + localSize - 1) / localSize;

      // Quarter resolution indirect illumination pass
      uint32_t currentPass = 0;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      // Reconstruction pass 1
      currentPass = 1;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      // Reconstruction pass 2
      currentPass = 2;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      // Reconstruction pass 3
      currentPass = 3;
      rsmUniformBuffer.SubData(currentPass, offsetof(RSMUniforms, currentPass));
      Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    }
  }
  Fwog::EndCompute();

  // shading pass (full screen tri)
  Fwog::BeginSwapchainRendering({
    .viewport =
      Fwog::Viewport{
        .drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}},
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      },
    .clearColorOnLoad = false,
    .clearColorValue = {.0f, .0f, .0f, 1.0f},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  });
  {
    Fwog::ScopedDebugMarker marker("Shading");
    Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
    Fwog::Cmd::BindSampledImage(0, *frame.gcolorTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(1, *frame.gnormalTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(2, *frame.gdepthTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(3, *frame.indirectLightingTex, nearestSampler);
    Fwog::Cmd::BindSampledImage(4, rdepthTex, rsmShadowSampler);
    Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer, 0, globalUniformsBuffer.Size());
    Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer, 0, shadingUniformsBuffer.Size());
    Fwog::Cmd::Draw(3, 1, 0, 0);

    Fwog::Texture* tex{};
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
      tex = &frame.gcolorTex.value();
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
      tex = &frame.gnormalTex.value();
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
      tex = &frame.gdepthTex.value();
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
      tex = &frame.indirectLightingTex.value();
    if (tex)
    {
      Fwog::Cmd::BindGraphicsPipeline(debugTexturePipeline);
      Fwog::Cmd::BindSampledImage(0, *tex, nearestSampler);
      Fwog::Cmd::Draw(3, 1, 0, 0);
    }
  }
  Fwog::EndRendering();
}

void DeferredApplication::OnGui(double dt)
{
  ImGui::Begin("Deferred");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Indirect Illumination: %f ms", illuminationTime);

  ImGui::Checkbox("Filter RSM", &rsmFiltered);
  ImGui::SliderInt("RSM samples", &rsmSamples, 1, 400);
  ImGui::SliderInt("Filtered RSM samples", &rsmFilteredSamples, 1, 20);
  rsmUniforms.samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples);
  ImGui::SliderFloat("rMax", &rsmUniforms.rMax, 0.02f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("Sun Angle", &sunPosition, -1.2f, 2.1f);

  ImGui::BeginTabBar("tabbed");
  if (ImGui::BeginTabItem("G-Buffers"))
  {
    float aspect = float(windowWidth) / windowHeight;
    glTextureParameteri(frame.gcolorTex.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gcolorTex.value().Handle())),
                       {100 * aspect, 100},
                       {0, 1},
                       {1, 0});
    ImGui::SameLine();
    glTextureParameteri(frame.gnormalTex.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gnormalTex.value().Handle())),
                       {100 * aspect, 100},
                       {0, 1},
                       {1, 0});
    glTextureParameteri(frame.gdepthTex.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gdepthTex.value().Handle())),
                       {100 * aspect, 100},
                       {0, 1},
                       {1, 0});
    ImGui::SameLine();
    glTextureParameteri(frame.indirectLightingTex.value().Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.indirectLightingTex.value().Handle())),
                       {100 * aspect, 100},
                       {0, 1},
                       {1, 0});
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("RSM Buffers"))
  {
    glTextureParameteri(rdepthTex.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rdepthTex.Handle())),
                 {100, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    glTextureParameteri(rnormalTex.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rnormalTex.Handle())),
                 {100, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    glTextureParameteri(rfluxTex.Handle(), GL_TEXTURE_SWIZZLE_A, GL_ONE);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rfluxTex.Handle())),
                 {100, 100},
                 {0, 1}, {1, 0});
    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
  ImGui::End();
}

int main()
{
  auto appInfo = Application::CreateInfo{.name = "Deferred Example"};
  auto app = DeferredApplication(appInfo);
  app.Run();

  return 0;
}
