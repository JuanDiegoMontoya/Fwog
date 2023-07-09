#include "common/Application.h"
#include "common/RsmTechnique.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#include <GLFW/glfw3.h>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <array>
#include <optional>
#include <tuple>
#include <vector>

/* 02_deferred
 *
 * This example implements a deferred renderer to visualize a simple 3D box scene. Deferred rendering is a technique
 * which renders the scene in two main passes instead of one. The first pass draws material properties to multiple
 * render targets: normal, albedo, and depth. The second pass uses a full-screen shader and these material properties to
 * shade the scene.
 *
 * This example also implements the paper reflective shadow maps (RSM) by Carsten Dachsbacher and Marc Stamminger.
 * RSM is an extension of shadow maps which adds normals and radiant flux render targets to the shadow pass to form
 * an RSM. Then, the RSM can be treated as a grid of point lights which is sampled several times to approximate one
 * bounce of indirect illumination. Also implemented is an extension of RSM which improves sampling and adds an
 * edge-stopping a-trous filter to blur the shadows, producing higher quality and cheaper indirect illumination.
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
  glm::mat4 oldViewProj;
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
  float random;
};

////////////////////////////////////// Globals

static constexpr auto gCubeVertices = std::array<Vertex, 24>{
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

static constexpr auto gCubeIndices = std::array<uint16_t, 36>{
  0,  1,  2,  2,  3,  0,

  4,  5,  6,  6,  7,  4,

  8,  9,  10, 10, 11, 8,

  12, 13, 14, 14, 15, 12,

  16, 17, 18, 18, 19, 16,

  20, 21, 22, 22, 23, 20,
};

static constexpr auto sceneInputBindingDescs = std::array{
  Fwog::VertexInputBindingDescription{
    // position
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

static Fwog::GraphicsPipeline CreateScenePipeline()
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

static Fwog::GraphicsPipeline CreateShadowPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/SceneDeferred.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/RSMScene.frag.glsl"));

  return Fwog::GraphicsPipeline({
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true},
  });
}

static Fwog::GraphicsPipeline CreateShadingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/ShadeDeferred.frag.glsl"));

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
  float sunPosition = 0;
  float sunPosition2 = 0;

  // transient variables
  double illuminationTime = 0;
  uint32_t sceneInstanceCount = 0;

  // Resources tied to the swapchain/output size
  struct Frame
  {
    // g-buffer textures
    std::optional<Fwog::Texture> gAlbedo;
    std::optional<Fwog::Texture> gNormal;
    std::optional<Fwog::Texture> gDepth;
    std::optional<Fwog::Texture> gNormalPrev;
    std::optional<Fwog::Texture> gDepthPrev;
    std::optional<Fwog::Texture> gMotion;
    std::optional<RSM::RsmTechnique> rsm;

    // For debug drawing with ImGui
    std::optional<Fwog::TextureView> gAlbedoSwizzled;
    std::optional<Fwog::TextureView> gNormalSwizzled;
    std::optional<Fwog::TextureView> gDepthSwizzled;
    std::optional<Fwog::TextureView> gRsmIlluminanceSwizzled;
  };
  Frame frame{};

  // Buffers describing the scene's objects and geometry
  std::optional<Fwog::Buffer> vertexBuffer;
  std::optional<Fwog::Buffer> indexBuffer;
  std::optional<Fwog::Buffer> objectBuffer;

  // Reflective shadow map textures
  Fwog::Texture rsmFlux;
  Fwog::Texture rsmNormal;
  Fwog::Texture rsmDepth;

  // For debug drawing with ImGui
  Fwog::TextureView rsmFluxSwizzled;
  Fwog::TextureView rsmNormalSwizzled;
  Fwog::TextureView rsmDepthSwizzled;

  ShadingUniforms shadingUniforms;
  GlobalUniforms globalUniforms{};
  uint64_t frameIndex = 0;

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;
  Fwog::TypedBuffer<ShadingUniforms> shadingUniformsBuffer;

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline rsmScenePipeline;
  Fwog::GraphicsPipeline shadingPipeline;
  Fwog::GraphicsPipeline debugTexturePipeline;
};

DeferredApplication::DeferredApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    // Create RSM textures
    rsmFlux(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R11G11B10_FLOAT)),
    rsmNormal(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::R16G16B16_SNORM)),
    rsmDepth(Fwog::CreateTexture2D({gShadowmapWidth, gShadowmapHeight}, Fwog::Format::D16_UNORM)),
    rsmFluxSwizzled(rsmFlux.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    rsmNormalSwizzled(rsmNormal.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    rsmDepthSwizzled(rsmDepth.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    // Create constant-size buffers
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    shadingUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    // Create the pipelines used in the application
    scenePipeline(CreateScenePipeline()),
    rsmScenePipeline(CreateShadowPipeline()),
    shadingPipeline(CreateShadingPipeline()),
    debugTexturePipeline(CreateDebugTexturePipeline())
{
  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  cursorIsActive = true;

  cameraSpeed = 1.0f;

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
  frame.gAlbedo = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_SRGB);
  frame.gNormal = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepth = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_UNORM);
  frame.gNormalPrev = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepthPrev = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_UNORM);
  frame.gMotion = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R16G16_FLOAT);

  frame.rsm = RSM::RsmTechnique(newWidth, newHeight);

  // create debug views
  frame.gAlbedoSwizzled = frame.gAlbedo->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gNormalSwizzled = frame.gNormal->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gDepthSwizzled = frame.gDepth->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gRsmIlluminanceSwizzled = frame.rsm->GetIndirectLighting().CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
}

void DeferredApplication::OnUpdate([[maybe_unused]] double dt)
{
  frameIndex++;
}

void DeferredApplication::OnRender([[maybe_unused]] double dt)
{
  std::swap(frame.gDepth, frame.gDepthPrev);
  std::swap(frame.gNormal, frame.gNormalPrev);

  shadingUniforms = ShadingUniforms{
    .sunDir = glm::normalize(glm::rotate(sunPosition, glm::vec3{1, 0, 0}) *
                             glm::rotate(sunPosition2, glm::vec3(0, 1, 0)) * glm::vec4{-.1, -.3, -.6, 0}),
    .sunStrength = glm::vec4{2, 2, 2, 0},
  };

  const auto proj = glm::perspective(glm::radians(70.f), windowWidth / (float)windowHeight, 0.1f, 5.f);
  glm::mat4 viewProj = proj * mainCamera.GetViewMatrix();

  globalUniforms.oldViewProj = frameIndex == 1 ? viewProj : globalUniforms.viewProj;
  globalUniforms.proj = proj;
  globalUniforms.viewProj = viewProj;
  globalUniformsBuffer.UpdateData(globalUniforms);

  glm::vec3 eye = glm::vec3{shadingUniforms.sunDir * -5.f};
  float eyeWidth = 2.5f;
  // shadingUniforms.viewPos = glm::vec4(camera.position, 0);
  auto projtemp = glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, .1f, 10.f);
  shadingUniforms.sunViewProj = projtemp * glm::lookAt(eye, glm::vec3(0), glm::vec3{0, 1, 0});
  shadingUniformsBuffer.UpdateData(shadingUniforms);

  Fwog::SamplerState ss;
  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  // Render scene geometry to the g-buffer
  // DONT_CARE indicates that the previous contents can be discarded before rendering
  auto gAlbedoAttachment = Fwog::RenderColorAttachment{
    .texture = frame.gAlbedo.value(),
    .loadOp = Fwog::AttachmentLoadOp::DONT_CARE,
  };
  auto gNormalAttachment = Fwog::RenderColorAttachment{
    .texture = frame.gNormal.value(),
    .loadOp = Fwog::AttachmentLoadOp::DONT_CARE,
  };
  auto gMotionAttachment = Fwog::RenderColorAttachment{
    .texture = frame.gMotion.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {0.f, 0.f},
  };
  auto gDepthAttachment = Fwog::RenderDepthStencilAttachment{
    .texture = frame.gDepth.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.depth = 1.0f},
  };
  Fwog::RenderColorAttachment cgAttachments[] = {gAlbedoAttachment, gNormalAttachment, gMotionAttachment};
  Fwog::Render(
    {
      .name = "Base Pass",
      .colorAttachments = cgAttachments,
      .depthAttachment = gDepthAttachment,
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
      Fwog::Cmd::BindIndexBuffer(*indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer);
      Fwog::Cmd::BindStorageBuffer(1, *objectBuffer);
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), sceneInstanceCount, 0, 0, 0);
    });

  globalUniforms.viewProj = shadingUniforms.sunViewProj;
  globalUniformsBuffer.UpdateData(globalUniforms);

  // Shadow map (RSM) scene pass
  auto rcolorAttachment = Fwog::RenderColorAttachment{
    .texture = rsmFlux,
    .loadOp = Fwog::AttachmentLoadOp::DONT_CARE,
  };
  auto rnormalAttachment = Fwog::RenderColorAttachment{
    .texture = rsmNormal,
    .loadOp = Fwog::AttachmentLoadOp::DONT_CARE,
  };
  auto rdepthAttachment = Fwog::RenderDepthStencilAttachment{
    .texture = rsmDepth,
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.depth = 1.0f},
  };
  Fwog::RenderColorAttachment crAttachments[] = {rcolorAttachment, rnormalAttachment};
  Fwog::Render(
    {
      .name = "RSM Scene",
      .colorAttachments = crAttachments,
      .depthAttachment = rdepthAttachment,
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(rsmScenePipeline);
      Fwog::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
      Fwog::Cmd::BindIndexBuffer(*indexBuffer, Fwog::IndexType::UNSIGNED_SHORT);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer);
      Fwog::Cmd::BindStorageBuffer(1, *objectBuffer);
      Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(gCubeIndices.size()), sceneInstanceCount, 0, 0, 0);
    });

  globalUniforms.viewProj = viewProj;
  globalUniforms.invViewProj = glm::inverse(viewProj);
  globalUniformsBuffer.UpdateData(globalUniforms);

  {
    static Fwog::TimerQueryAsync timer(5);
    if (auto t = timer.PopTimestamp())
    {
      illuminationTime = *t / 10e5;
    }
    Fwog::TimerScoped scopedTimer(timer);

    auto rsmCameraUniforms = RSM::CameraUniforms{
      .viewProj = viewProj,
      .invViewProj = glm::inverse(viewProj),
      .proj = proj,
      .cameraPos = glm::vec4(mainCamera.position, 0),
      .viewDir = mainCamera.GetForwardDir(),
    };

    frame.rsm->ComputeIndirectLighting(shadingUniforms.sunViewProj,
                                       rsmCameraUniforms,
                                       frame.gAlbedo.value(),
                                       frame.gNormal.value(),
                                       frame.gDepth.value(),
                                       rsmFlux,
                                       rsmNormal,
                                       rsmDepth,
                                       frame.gDepthPrev.value(),
                                       frame.gNormalPrev.value(),
                                       frame.gMotion.value());
  }

  // shading pass (full screen tri)
  Fwog::RenderToSwapchain(
    {
      .name = "Shading",
      .viewport =
        Fwog::Viewport{
          .drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}},
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
        },
      .colorLoadOp = Fwog::AttachmentLoadOp::CLEAR,
      .clearColorValue = {.1f, .3f, .5f, 0.0f},
      .depthLoadOp = Fwog::AttachmentLoadOp::DONT_CARE,
      .stencilLoadOp = Fwog::AttachmentLoadOp::DONT_CARE,
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, *frame.gAlbedo, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, *frame.gNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, *frame.gDepth, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, frame.rsm->GetIndirectLighting(), nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rsmDepth, nearestSampler);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer);
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
    });
}

void DeferredApplication::OnGui(double dt)
{
  ImGui::Begin("Deferred");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Indirect Illumination: %f ms", illuminationTime);

  ImGui::SliderFloat("Sun Angle", &sunPosition, -1.2f, 2.1f);
  ImGui::SliderFloat("Sun Angle 2", &sunPosition2, -3.142f, 3.142f);

  ImGui::Separator();

  frame.rsm->DrawGui();

  ImGui::BeginTabBar("tabbed");
  if (ImGui::BeginTabItem("G-Buffers"))
  {
    float aspect = float(windowWidth) / windowHeight;
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gAlbedoSwizzled.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gNormalSwizzled.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gDepthSwizzled.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.gRsmIlluminanceSwizzled.value().Handle())),
                 {100 * aspect, 100},
                 {0, 1},
                 {1, 0});
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("RSM Buffers"))
  {
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmDepthSwizzled.Handle())), {100, 100}, {0, 1}, {1, 0});
    ImGui::SameLine();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmNormalSwizzled.Handle())),
                 {100, 100},
                 {0, 1},
                 {1, 0});
    ImGui::SameLine();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rsmFluxSwizzled.Handle())), {100, 100}, {0, 1}, {1, 0});
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
