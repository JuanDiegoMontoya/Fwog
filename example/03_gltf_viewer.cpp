#include "common/Application.h"
#include "common/RsmTechnique.h"
#include "common/SceneLoader.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#ifdef FWOG_FSR2_ENABLE
  #include "src/ffx-fsr2-api/ffx_fsr2.h"
  #include "src/ffx-fsr2-api/gl/ffx_fsr2_gl.h"
#endif

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stb_image.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <charconv>
#include <cstring>
#include <exception>
#include <memory>
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

static glm::uint pcg_hash(glm::uint seed)
{
  glm::uint state = seed * 747796405u + 2891336453u;
  glm::uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// Used to advance the PCG state.
static glm::uint rand_pcg(glm::uint& rng_state)
{
  glm::uint state = rng_state;
  rng_state = rng_state * 747796405u + 2891336453u;
  glm::uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// Advances the prng state and returns the corresponding random float.
static float rng(glm::uint& state)
{
  glm::uint x = rand_pcg(state);
  state = x;
  return float(x) * glm::uintBitsToFloat(0x2f800004u);
}

////////////////////////////////////// Types

struct ObjectUniforms
{
  glm::mat4 model;
};

struct GlobalUniforms
{
  glm::mat4 viewProj;
  glm::mat4 oldViewProjUnjittered;
  glm::mat4 viewProjUnjittered;
  glm::mat4 invViewProj;
  glm::mat4 proj;
  glm::vec4 cameraPos;
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
  glm::mat4 sunView;
  glm::mat4 sunProj;
  glm::vec2 random;
};

struct ShadowUniforms
{
  uint32_t shadowMode = 0; // 0 = PCF, 1 = SMRT

  // PCF stuff
  uint32_t pcfSamples = 8;
  float pcfRadius = 0.002f;

  // SMRT stuff
  uint32_t shadowRays = 7;
  uint32_t stepsPerRay = 7;
  float rayStepSize = 0.1f;
  float heightmapThickness = 0.5f;
  float sourceAngleRad = 0.05f;
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

static Fwog::GraphicsPipeline CreatePostprocessingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, Application::LoadFile("shaders/FullScreenTri.vert.glsl"));
  auto fs =
    Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, Application::LoadFile("shaders/TonemapAndDither.frag.glsl"));
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
  static constexpr int gShadowmapWidth = 2048;
  static constexpr int gShadowmapHeight = 2048;

  double illuminationTime = 0;
  double fsr2Time = 0;

  // scene parameters
  float sunPosition = -1.127f;
  float sunPosition2 = 0;
  float sunStrength = 50;
  glm::vec3 sunColor = {1, 1, 1};

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
    std::optional<Fwog::Texture> colorHdrRenderRes;
    std::optional<Fwog::Texture> colorHdrWindowRes;
    std::optional<Fwog::Texture> colorLdrWindowRes;
    std::optional<RSM::RsmTechnique> rsm;

    // For debug drawing with ImGui
    std::optional<Fwog::TextureView> gAlbedoSwizzled;
    std::optional<Fwog::TextureView> gNormalSwizzled;
    std::optional<Fwog::TextureView> gDepthSwizzled;
    std::optional<Fwog::TextureView> gRsmIlluminanceSwizzled;
  };
  Frame frame{};

  // Reflective shadow map textures
  Fwog::Texture rsmFlux;
  Fwog::Texture rsmNormal;
  Fwog::Texture rsmDepth;

  // For debug drawing with ImGui
  Fwog::TextureView rsmFluxSwizzled;
  Fwog::TextureView rsmNormalSwizzled;
  Fwog::TextureView rsmDepthSwizzled;

  ShadingUniforms shadingUniforms{};
  ShadowUniforms shadowUniforms{};
  GlobalUniforms mainCameraUniforms{};

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;
  Fwog::TypedBuffer<ShadingUniforms> shadingUniformsBuffer;
  Fwog::TypedBuffer<ShadowUniforms> shadowUniformsBuffer;
  Fwog::TypedBuffer<Utility::GpuMaterial> materialUniformsBuffer;
  Fwog::TypedBuffer<glm::mat4> rsmUniforms;

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline rsmScenePipeline;
  Fwog::GraphicsPipeline shadingPipeline;
  Fwog::GraphicsPipeline postprocessingPipeline;
  Fwog::GraphicsPipeline debugTexturePipeline;

  // Scene
  Utility::Scene scene;
  std::optional<Fwog::TypedBuffer<Light>> lightBuffer;
  std::optional<Fwog::TypedBuffer<ObjectUniforms>> meshUniformBuffer;

  // Post processing
  std::optional<Fwog::Texture> noiseTexture;

  uint32_t renderWidth;
  uint32_t renderHeight;
  uint32_t frameIndex = 0;
  uint32_t seed = pcg_hash(17);

#ifdef FWOG_FSR2_ENABLE
  // FSR 2
  bool fsr2Enable = true;
  bool fsr2FirstInit = true;
  float fsr2Sharpness = 0;
  float fsr2Ratio = 1.7f; // FFX_FSR2_QUALITY_MODE_BALANCED
  FfxFsr2Context fsr2Context;
  std::unique_ptr<char[]> fsr2ScratchMemory;
#else
  const bool fsr2Enable = false;
#endif

  // Magnifier
  bool magnifierLock = false;
  float magnifierScale = 0.0173f;
  glm::vec2 lastCursorPos = {};
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
    rsmFluxSwizzled(rsmFlux.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    rsmNormalSwizzled(rsmNormal.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    rsmDepthSwizzled(rsmDepth.CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE})),
    // Create constant-size buffers
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    shadingUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    shadowUniformsBuffer(shadowUniforms, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    materialUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    rsmUniforms(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    // Create the pipelines used in the application
    scenePipeline(CreateScenePipeline()),
    rsmScenePipeline(CreateShadowPipeline()),
    shadingPipeline(CreateShadingPipeline()),
    postprocessingPipeline(CreatePostprocessingPipeline()),
    debugTexturePipeline(CreateDebugTexturePipeline())
{
  int x = 0;
  int y = 0;
  const auto noise = stbi_load("textures/bluenoise32.png", &x, &y, nullptr, 4);
  assert(noise);
  noiseTexture = Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
  noiseTexture->UpdateImage({
    .extent = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
    .format = Fwog::UploadFormat::RGBA,
    .type = Fwog::UploadType::UBYTE,
    .pixels = noise,
  });
  stbi_image_free(noise);

  ImGui::GetIO().Fonts->AddFontFromFileTTF("textures/RobotoCondensed-Regular.ttf", 18);

  cursorIsActive = true;

  cameraSpeed = 2.5f;
  mainCamera.position.y = 1;

  if (!filename)
  {
    Utility::LoadModelFromFile(scene, "models/simple_scene.glb", glm::mat4{.125}, true);
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

  if (!lights.empty())
  {
    lightBuffer.emplace(lights, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  }

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
#ifdef FWOG_FSR2_ENABLE
  // create FSR 2 context
  if (fsr2Enable)
  {
    if (!fsr2FirstInit)
    {
      ffxFsr2ContextDestroy(&fsr2Context);
    }
    frameIndex = 0;
    fsr2FirstInit = false;
    renderWidth = newWidth / fsr2Ratio;
    renderHeight = newHeight / fsr2Ratio;
    FfxFsr2ContextDescription contextDesc{
      .flags = FFX_FSR2_ENABLE_DEBUG_CHECKING | FFX_FSR2_ENABLE_AUTO_EXPOSURE | FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE |
               FFX_FSR2_ALLOW_NULL_DEVICE_AND_COMMAND_LIST,
      .maxRenderSize = {renderWidth, renderHeight},
      .displaySize = {newWidth, newHeight},
      .fpMessage =
        [](FfxFsr2MsgType type, const wchar_t* message)
      {
        char cstr[256] = {};
        wcstombs_s(nullptr, cstr, sizeof(cstr), message, sizeof(cstr));
        cstr[255] = '\0';
        printf("FSR 2 message (type=%d): %s\n", type, cstr);
      },
    };
    fsr2ScratchMemory = std::make_unique<char[]>(ffxFsr2GetScratchMemorySizeGL());
    ffxFsr2GetInterfaceGL(&contextDesc.callbacks, fsr2ScratchMemory.get(), ffxFsr2GetScratchMemorySizeGL(), glfwGetProcAddress);
    ffxFsr2ContextCreate(&fsr2Context, &contextDesc);
  }
  else
#endif
  {
    renderWidth = newWidth;
    renderHeight = newHeight;
  }

  // create gbuffer textures and render info
  frame.gAlbedo = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::R8G8B8A8_SRGB, "gAlbedo");
  frame.gNormal = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::R16G16B16_SNORM, "gNormal");
  frame.gDepth = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::D32_FLOAT, "gDepth");
  frame.gNormalPrev = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::R16G16B16_SNORM);
  frame.gDepthPrev = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::D32_FLOAT);
  frame.gMotion = Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::R16G16_FLOAT, "gMotion");
  frame.colorHdrRenderRes =
    Fwog::CreateTexture2D({renderWidth, renderHeight}, Fwog::Format::R11G11B10_FLOAT, "colorHdrRenderRes");
  frame.colorHdrWindowRes =
    Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R11G11B10_FLOAT, "colorHdrWindowRes");
  frame.colorLdrWindowRes = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_UNORM, "colorLdrWindowRes");

  if (!frame.rsm)
  {
    frame.rsm = RSM::RsmTechnique(renderWidth, renderHeight);
  }
  else
  {
    frame.rsm->SetResolution(renderWidth, renderHeight);
  }

  // create debug views
  frame.gAlbedoSwizzled = frame.gAlbedo->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gNormalSwizzled = frame.gNormal->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gDepthSwizzled = frame.gDepth->CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
  frame.gRsmIlluminanceSwizzled = frame.rsm->GetIndirectLighting().CreateSwizzleView({.a = Fwog::ComponentSwizzle::ONE});
}

void GltfViewerApplication::OnUpdate([[maybe_unused]] double dt)
{
  frameIndex++;

  if (fsr2Enable)
  {
    shadingUniforms.random = {rng(seed), rng(seed)};
  }
  else
  {
    shadingUniforms.random = {0, 0};
  }
}

static glm::vec2 GetJitterOffset(uint32_t frameIndex, uint32_t renderWidth, uint32_t renderHeight, uint32_t windowWidth)
{
#ifdef FWOG_FSR2_ENABLE
  float jitterX{};
  float jitterY{};
  ffxFsr2GetJitterOffset(&jitterX, &jitterY, frameIndex, ffxFsr2GetJitterPhaseCount(renderWidth, windowWidth));
  return {2.0f * jitterX / static_cast<float>(renderWidth), 2.0f * jitterY / static_cast<float>(renderHeight)};
#else
  return {0, 0};
#endif
}

void GltfViewerApplication::OnRender([[maybe_unused]] double dt)
{
  std::swap(frame.gDepth, frame.gDepthPrev);
  std::swap(frame.gNormal, frame.gNormalPrev);

  shadingUniforms.sunDir = glm::normalize(glm::rotate(sunPosition, glm::vec3{1, 0, 0}) *
                                          glm::rotate(sunPosition2, glm::vec3(0, 1, 0)) * glm::vec4{-.1, -.3, -.6, 0});
  shadingUniforms.sunStrength = glm::vec4{sunStrength * sunColor, 0};

#ifdef FWOG_FSR2_ENABLE
  const float fsr2LodBias = fsr2Enable ? log2(float(renderWidth) / float(windowWidth)) - 1.0 : 0;
#else
  const float fsr2LodBias = 0;
#endif

  Fwog::SamplerState ss;

  ss.minFilter = Fwog::Filter::NEAREST;
  ss.magFilter = Fwog::Filter::NEAREST;
  ss.addressModeU = Fwog::AddressMode::REPEAT;
  ss.addressModeV = Fwog::AddressMode::REPEAT;
  auto nearestSampler = Fwog::Sampler(ss);

  ss.lodBias = 0;
  ss.compareEnable = true;
  ss.compareOp = Fwog::CompareOp::LESS;
  auto shadowSampler = Fwog::Sampler(ss);

  constexpr float cameraNear = 0.1f;
  constexpr float cameraFar = 100.0f;
  constexpr float cameraFovY = glm::radians(70.f);
  const auto jitterOffset = fsr2Enable ? GetJitterOffset(frameIndex, renderWidth, renderHeight, windowWidth) : glm::vec2{};
  const auto jitterMatrix = glm::translate(glm::mat4(1), glm::vec3(jitterOffset, 0));
  const auto projUnjittered = glm::perspectiveNO(cameraFovY, renderWidth / (float)renderHeight, cameraNear, cameraFar);
  const auto projJittered = jitterMatrix * projUnjittered;

  // Set global uniforms
  const auto viewProj = projJittered * mainCamera.GetViewMatrix();
  const auto viewProjUnjittered = projUnjittered * mainCamera.GetViewMatrix();
  mainCameraUniforms.oldViewProjUnjittered = frameIndex == 1 ? viewProjUnjittered : mainCameraUniforms.viewProjUnjittered;
  mainCameraUniforms.viewProjUnjittered = viewProjUnjittered;
  mainCameraUniforms.viewProj = viewProj;
  mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
  mainCameraUniforms.proj = projJittered;
  mainCameraUniforms.cameraPos = glm::vec4(mainCamera.position, 0.0);

  globalUniformsBuffer.UpdateData(mainCameraUniforms);

  shadowUniformsBuffer.UpdateData(shadowUniforms);

  glm::vec3 eye = glm::vec3{shadingUniforms.sunDir * -5.f};
  float eyeWidth = 7.0f;
  // shadingUniforms.viewPos = glm::vec4(camera.position, 0);
  shadingUniforms.sunProj = glm::orthoZO(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, -100.0f, 100.f);
  shadingUniforms.sunView = glm::lookAt(eye, glm::vec3(0), glm::vec3{0, 1, 0});
  shadingUniforms.sunViewProj = shadingUniforms.sunProj * shadingUniforms.sunView;
  shadingUniformsBuffer.UpdateData(shadingUniforms);

  // Render scene geometry to the g-buffer
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
    .clearValue = {0.f, 0.f, 0.f, 0.f},
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
      .viewport =
        Fwog::Viewport{
          .drawRect = {{0, 0}, {renderWidth, renderHeight}},
          .depthRange = Fwog::ClipDepthRange::NEGATIVE_ONE_TO_ONE,
        },
      .colorAttachments = cgAttachments,
      .depthAttachment = gDepthAttachment,
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer);

      Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer);
      for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
      {
        const auto& mesh = scene.meshes[i];
        const auto& material = scene.materials[mesh.materialIdx];
        materialUniformsBuffer.UpdateData(material.gpuMaterial);
        if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
        {
          const auto& textureSampler = material.albedoTextureSampler.value();
          auto sampler = textureSampler.sampler;
          sampler.lodBias = fsr2LodBias;
          Fwog::Cmd::BindSampledImage(0, textureSampler.texture, Fwog::Sampler(sampler));
        }
        Fwog::Cmd::BindVertexBuffer(0, mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
        Fwog::Cmd::BindIndexBuffer(mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
        Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer.Size()) / sizeof(uint32_t), 1, 0, 0, i);
      }
    });

  rsmUniforms.UpdateData(shadingUniforms.sunViewProj);

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
      Fwog::Cmd::BindUniformBuffer(0, rsmUniforms);
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(2, materialUniformsBuffer);

      Fwog::Cmd::BindStorageBuffer(1, *meshUniformBuffer, 0);
      for (uint32_t i = 0; i < static_cast<uint32_t>(scene.meshes.size()); i++)
      {
        const auto& mesh = scene.meshes[i];
        const auto& material = scene.materials[mesh.materialIdx];
        materialUniformsBuffer.UpdateData(material.gpuMaterial);
        if (material.gpuMaterial.flags & Utility::MaterialFlagBit::HAS_BASE_COLOR_TEXTURE)
        {
          const auto& textureSampler = material.albedoTextureSampler.value();
          Fwog::Cmd::BindSampledImage(0, textureSampler.texture, Fwog::Sampler(textureSampler.sampler));
        }
        Fwog::Cmd::BindVertexBuffer(0, mesh.vertexBuffer, 0, sizeof(Utility::Vertex));
        Fwog::Cmd::BindIndexBuffer(mesh.indexBuffer, Fwog::IndexType::UNSIGNED_INT);
        Fwog::Cmd::DrawIndexed(static_cast<uint32_t>(mesh.indexBuffer.Size()) / sizeof(uint32_t), 1, 0, 0, i);
      }
    });

  auto rsmCameraUniforms = RSM::CameraUniforms{
    .viewProj = projUnjittered * mainCamera.GetViewMatrix(),
    .invViewProj = glm::inverse(viewProjUnjittered),
    .proj = projUnjittered,
    .cameraPos = glm::vec4(mainCamera.position, 0),
    .viewDir = mainCamera.GetForwardDir(),
    .jitterOffset = jitterOffset,
    .lastFrameJitterOffset =
      fsr2Enable ? GetJitterOffset(frameIndex - 1, renderWidth, renderHeight, windowWidth) : glm::vec2{},
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
                                       frame.gNormalPrev.value(),
                                       frame.gMotion.value());
  }

  // clear cluster indices atomic counter
  // clusterIndicesBuffer.ClearSubData(0, sizeof(uint32_t), Fwog::Format::R32_UINT, Fwog::UploadFormat::R, Fwog::UploadType::UINT, &zero);

  // record active clusters
  // TODO

  // light culling+cluster assignment

  //

  // shading pass (full screen tri)

  auto shadingColorAttachment = Fwog::RenderColorAttachment{
    .texture = frame.colorHdrRenderRes.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.1f, .3f, .5f, 0.0f},
  };
  Fwog::Render(
    {
      .name = "Shading",
      .colorAttachments = {&shadingColorAttachment, 1},
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(shadingPipeline);
      Fwog::Cmd::BindSampledImage(0, *frame.gAlbedo, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, *frame.gNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, *frame.gDepth, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, frame.rsm->GetIndirectLighting(), nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rsmDepth, nearestSampler);
      Fwog::Cmd::BindSampledImage(5, rsmDepth, shadowSampler);
      Fwog::Cmd::BindUniformBuffer(0, globalUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(1, shadingUniformsBuffer);
      Fwog::Cmd::BindUniformBuffer(2, shadowUniformsBuffer);
      if (lightBuffer)
      {
        Fwog::Cmd::BindStorageBuffer(0, *lightBuffer);
      }
      Fwog::Cmd::Draw(3, 1, 0, 0);
    });

#ifdef FWOG_FSR2_ENABLE
  if (fsr2Enable)
  {
    Fwog::Compute(
      "FSR 2",
      [&]
      {
        static Fwog::TimerQueryAsync timer(5);
        if (auto t = timer.PopTimestamp())
        {
          fsr2Time = *t / 10e5;
        }
        Fwog::TimerScoped scopedTimer(timer);

        if (frameIndex == 1)
        {
          dt = 17.0 / 1000.0;
        }

        float jitterX{};
        float jitterY{};
        ffxFsr2GetJitterOffset(&jitterX, &jitterY, frameIndex, ffxFsr2GetJitterPhaseCount(renderWidth, windowWidth));

        FfxFsr2DispatchDescription dispatchDesc{
          .color = ffxGetTextureResourceGL(frame.colorHdrRenderRes->Handle(), renderWidth, renderHeight, GL_R11F_G11F_B10F),
          .depth = ffxGetTextureResourceGL(frame.gDepth->Handle(), renderWidth, renderHeight, GL_DEPTH_COMPONENT32F),
          .motionVectors = ffxGetTextureResourceGL(frame.gMotion->Handle(), renderWidth, renderHeight, GL_RG16F),
          .exposure = {},
          .reactive = {},
          .transparencyAndComposition = {},
          .output =
            ffxGetTextureResourceGL(frame.colorHdrWindowRes->Handle(), windowWidth, windowHeight, GL_R11F_G11F_B10F),
          .jitterOffset = {jitterX, jitterY},
          .motionVectorScale = {float(renderWidth), float(renderHeight)},
          .renderSize = {renderWidth, renderHeight},
          .enableSharpening = fsr2Sharpness != 0,
          .sharpness = fsr2Sharpness,
          .frameTimeDelta = static_cast<float>(dt * 1000.0),
          .preExposure = 1,
          .reset = false,
          .cameraNear = cameraNear,
          .cameraFar = cameraFar,
          .cameraFovAngleVertical = cameraFovY,
          .viewSpaceToMetersFactor = 1,
          .deviceDepthNegativeOneToOne = false,
        };

        if (auto err = ffxFsr2ContextDispatch(&fsr2Context, &dispatchDesc); err != FFX_OK)
        {
          printf("FSR 2 error: %d\n", err);
        }
      });
  }
  Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
#endif

  const auto ppAttachment = Fwog::RenderColorAttachment{
    .texture = frame.colorLdrWindowRes.value(),
    .loadOp = Fwog::AttachmentLoadOp::DONT_CARE,
  };

  Fwog::Render(
    {
      .name = "Postprocessing",
      .colorAttachments = {&ppAttachment, 1},
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(postprocessingPipeline);
      Fwog::Cmd::BindSampledImage(0,
                                  fsr2Enable ? frame.colorHdrWindowRes.value() : frame.colorHdrRenderRes.value(),
                                  nearestSampler);
      Fwog::Cmd::BindSampledImage(1, noiseTexture.value(), nearestSampler);
      Fwog::Cmd::Draw(3, 1, 0, 0);
    });

  Fwog::RenderToSwapchain(
    {
      .name = "Copy to swapchain",
      .viewport =
        Fwog::Viewport{
          .drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}},
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
        },
      .colorLoadOp = Fwog::AttachmentLoadOp::DONT_CARE,
      .depthLoadOp = Fwog::AttachmentLoadOp::DONT_CARE,
      .stencilLoadOp = Fwog::AttachmentLoadOp::DONT_CARE,
      .enableSrgb = false,
    },
    [&]
    {
      const Fwog::Texture* tex = &frame.colorLdrWindowRes.value();
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

void GltfViewerApplication::OnGui([[maybe_unused]] double dt)
{
  ImGui::Begin("glTF Viewer");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Indirect Illumination: %f ms", illuminationTime);
  ImGui::Text("FSR 2: %f ms", fsr2Time);

  ImGui::SliderFloat("Sun Angle", &sunPosition, -2.7f, 0.5f);
  ImGui::SliderFloat("Sun Angle 2", &sunPosition2, -3.142f, 3.142f);
  ImGui::ColorEdit3("Sun Color", &sunColor[0], ImGuiColorEditFlags_Float);
  ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 50);

  ImGui::Separator();

  frame.rsm->DrawGui();

  ImGui::Separator();

  ImGui::Text("FSR 2");
#ifdef FWOG_FSR2_ENABLE
  if (ImGui::Checkbox("Enable FSR 2", &fsr2Enable))
  {
    OnWindowResize(windowWidth, windowHeight);
  }

  if (!fsr2Enable)
  {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }

  float ratio = fsr2Ratio;
  if (ImGui::RadioButton("AA (1.0x)", ratio == 1.0f))
    ratio = 1.0f;
  if (ImGui::RadioButton("Ultra Quality (1.3x)", ratio == 1.3f))
    ratio = 1.3f;
  if (ImGui::RadioButton("Quality (1.5x)", ratio == 1.5f))
    ratio = 1.5f;
  if (ImGui::RadioButton("Balanced (1.7x)", ratio == 1.7f))
    ratio = 1.7f;
  if (ImGui::RadioButton("Performance (2.0x)", ratio == 2.0f))
    ratio = 2.0f;
  if (ImGui::RadioButton("Ultra Performance (3.0x)", ratio == 3.0f))
    ratio = 3.0f;
  ImGui::SliderFloat("RCAS Strength", &fsr2Sharpness, 0, 1);

  if (ratio != fsr2Ratio)
  {
    fsr2Ratio = ratio;
    OnWindowResize(windowWidth, windowHeight);
  }

  if (!fsr2Enable)
  {
    ImGui::PopStyleVar();
    ImGui::PopItemFlag();
  }
#else
  ImGui::Text("Compile with FWOG_FSR2_ENABLE defined to see FSR 2 options");
#endif

  ImGui::Separator();

  ImGui::Text("Shadow");

  auto SliderUint = [](const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max) -> bool
  {
    int tempv = static_cast<int>(*v);
    if (ImGui::SliderInt(label, &tempv, static_cast<int>(v_min), static_cast<int>(v_max)))
    {
      *v = static_cast<uint32_t>(tempv);
      return true;
    }
    return false;
  };

  int shadowMode = shadowUniforms.shadowMode;
  ImGui::RadioButton("PCF", &shadowMode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("SMRT", &shadowMode, 1);
  shadowUniforms.shadowMode = shadowMode;

  if (shadowMode == 0)
  {
    SliderUint("PCF Samples", &shadowUniforms.pcfSamples, 1, 16);
    ImGui::SliderFloat("PCF Radius", &shadowUniforms.pcfRadius, 0, 0.01f, "%.4f");
  }
  else if (shadowMode == 1)
  {
    SliderUint("Shadow Rays", &shadowUniforms.shadowRays, 1, 10);
    SliderUint("Steps Per Ray", &shadowUniforms.stepsPerRay, 1, 20);
    ImGui::SliderFloat("Ray Step Size", &shadowUniforms.rayStepSize, 0.01f, 1.0f);
    ImGui::SliderFloat("Heightmap Thickness", &shadowUniforms.heightmapThickness, 0.05f, 1.0f);
    ImGui::SliderFloat("Light Spread", &shadowUniforms.sourceAngleRad, 0.001f, 0.3f);
  }

  ImGui::BeginTabBar("tabbed");
  if (ImGui::BeginTabItem("G-Buffers"))
  {
    float aspect = float(renderWidth) / renderHeight;
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

  ImGui::Begin(
    ("Magnifier: " + std::string(magnifierLock ? "Locked (L, Space)" : "Unlocked (L, Space)") + "###mag").c_str());
  if (ImGui::GetKeyPressedAmount(ImGuiKey_KeypadSubtract, 10000, 1))
  {
    magnifierScale *= 1.5f;
  }
  if (ImGui::GetKeyPressedAmount(ImGuiKey_KeypadAdd, 10000, 1))
  {
    magnifierScale /= 1.5f;
  }
  float scale = 1.0f / magnifierScale;
  ImGui::SliderFloat("Scale (+, -)", &scale, 2.0f, 250.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
  magnifierScale = 1.0f / scale;
  if (ImGui::GetKeyPressedAmount(ImGuiKey_L, 10000, 1) || ImGui::GetKeyPressedAmount(ImGuiKey_Space, 10000, 1))
  {
    magnifierLock = !magnifierLock;
  }
  double x{}, y{};
  glfwGetCursorPos(window, &x, &y);
  glm::vec2 mp = magnifierLock ? lastCursorPos : glm::vec2{x, y};
  lastCursorPos = mp;
  mp.y = windowHeight - mp.y;
  mp /= glm::vec2(windowWidth, windowHeight);
  float ar = (float)windowWidth / (float)windowHeight;
  glm::vec2 uv0{mp.x - magnifierScale, mp.y + magnifierScale * ar};
  glm::vec2 uv1{mp.x + magnifierScale, mp.y - magnifierScale * ar};
  uv0 = glm::clamp(uv0, glm::vec2(0), glm::vec2(1));
  uv1 = glm::clamp(uv1, glm::vec2(0), glm::vec2(1));
  glTextureParameteri(frame.colorLdrWindowRes.value().Handle(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(frame.colorLdrWindowRes.value().Handle())),
               ImVec2(400, 400),
               ImVec2(uv0.x, uv0.y),
               ImVec2(uv1.x, uv1.y));
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

  auto appInfo = Application::CreateInfo{.name = "glTF Viewer Example", .vsync = false};
  auto app = GltfViewerApplication(appInfo, filename, scale, binary);
  app.Run();

  return 0;
}