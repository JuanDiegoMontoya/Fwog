#include "common.h"

#include <array>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>

#include <gsdf/BasicTypes.h>
#include <gsdf/Fence.h>
#include <gsdf/Rendering.h>
#include <gsdf/DebugMarker.h>
#include <gsdf/Timer.h>

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
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
};

struct alignas(16) RSMUniforms
{
  glm::mat4 sunViewProj;
  glm::mat4 invSunViewProj;
  glm::ivec2 targetDim;
  float rMax;
  uint32_t currentPass;
  uint32_t samples;
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

// scene parameters
uint32_t gRSMSamples = 400;
float gRMax = 0.08f;

constexpr int gShadowmapWidth = 1024;
constexpr int gShadowmapHeight = 1024;

std::array<Vertex, 24> gCubeVertices
{
  // front (+z)
  Vertex
  { { -0.5, -0.5, 0.5 }, { 0, 0, 1 }, { 0, 0 } },
  { {  0.5, -0.5, 0.5 }, { 0, 0, 1 }, { 1, 0 } },
  { {  0.5, 0.5,  0.5 }, { 0, 0, 1 }, { 1, 1 } },
  { { -0.5, 0.5,  0.5 }, { 0, 0, 1 }, { 0, 1 } },

  // back (-z)
  { { -0.5, 0.5,  -0.5 }, { 0, 0, -1 }, { 1, 1 } },
  { {  0.5, 0.5,  -0.5 }, { 0, 0, -1 }, { 0, 1 } },
  { {  0.5, -0.5, -0.5 }, { 0, 0, -1 }, { 0, 0 } },
  { { -0.5, -0.5, -0.5 }, { 0, 0, -1 }, { 1, 0 } },

  // left (-x)
  { { -0.5, -0.5,-0.5 }, { -1, 0, 0 }, { 0, 0 } },
  { { -0.5, -0.5, 0.5 }, { -1, 0, 0 }, { 1, 0 } },
  { { -0.5, 0.5,  0.5 }, { -1, 0, 0 }, { 1, 1 } },
  { { -0.5, 0.5, -0.5 }, { -1, 0, 0 }, { 0, 1 } },

  // right (+x)
  { { 0.5, 0.5,  -0.5 }, { 1, 0, 0 }, { 1, 1 } },
  { { 0.5, 0.5,   0.5 }, { 1, 0, 0 }, { 0, 1 } },
  { { 0.5, -0.5,  0.5 }, { 1, 0, 0 }, { 0, 0 } },
  { { 0.5, -0.5, -0.5 }, { 1, 0, 0 }, { 1, 0 } },

  // top (+y)
  { {-0.5, 0.5, 0.5 }, { 0, 1, 0 }, { 0, 0 } },
  { { 0.5, 0.5, 0.5 }, { 0, 1, 0 }, { 1, 0 } },
  { { 0.5, 0.5,-0.5 }, { 0, 1, 0 }, { 1, 1 } },
  { {-0.5, 0.5,-0.5 }, { 0, 1, 0 }, { 0, 1 } },

  // bottom (-y)
  { {-0.5, -0.5,-0.5 }, { 0, -1, 0 }, { 0, 0 } },
  { { 0.5, -0.5,-0.5 }, { 0, -1, 0 }, { 1, 0 } },
  { { 0.5, -0.5, 0.5 }, { 0, -1, 0 }, { 1, 1 } },
  { {-0.5, -0.5, 0.5 }, { 0, -1, 0 }, { 0, 1 } },
};

std::array<uint16_t, 36> gCubeIndices
{
  0, 1, 2,
  2, 3, 0,

  4, 5, 6,
  6, 7, 4,

  8, 9, 10,
  10, 11, 8,

  12, 13, 14,
  14, 15, 12,

  16, 17, 18,
  18, 19, 16,

  20, 21, 22,
  22, 23, 20,
};

std::array<GFX::VertexInputBindingDescription, 3> GetSceneInputBindingDescs()
{
  GFX::VertexInputBindingDescription descPos
  {
    .location = 0,
    .binding = 0,
    .format = GFX::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, position),
  };
  GFX::VertexInputBindingDescription descNormal
  {
    .location = 1,
    .binding = 0,
    .format = GFX::Format::R32G32B32_FLOAT,
    .offset = offsetof(Vertex, normal),
  };
  GFX::VertexInputBindingDescription descUV
  {
    .location = 2,
    .binding = 0,
    .format = GFX::Format::R32G32_FLOAT,
    .offset = offsetof(Vertex, uv),
  };

  return { descPos, descNormal, descUV };
}

GFX::RasterizationState GetDefaultRasterizationState()
{
  return GFX::RasterizationState
  {
    .depthClampEnable = false,
    .polygonMode = GFX::PolygonMode::FILL,
    .cullMode = GFX::CullMode::BACK,
    .frontFace = GFX::FrontFace::COUNTERCLOCKWISE,
    .depthBiasEnable = false,
    .lineWidth = 1.0f,
    .pointSize = 1.0f,
  };
}

GFX::GraphicsPipeline CreateScenePipeline()
{
  GLuint shader = Utility::CompileVertexFragmentProgram(
    Utility::LoadFile("shaders/SceneDeferred.vert.glsl"),
    Utility::LoadFile("shaders/SceneDeferred.frag.glsl"));

  GFX::InputAssemblyState inputAssembly
  {
    .topology = GFX::PrimitiveTopology::TRIANGLE_LIST,
    .primitiveRestartEnable = false,
  };

  auto inputDescs = GetSceneInputBindingDescs();
  GFX::VertexInputState vertexInput{ inputDescs };

  auto rasterization = GetDefaultRasterizationState();

  GFX::DepthStencilState depthStencil
  {
    .depthTestEnable = true,
    .depthWriteEnable = true,
    .depthCompareOp = GFX::CompareOp::LESS,
  };

  GFX::ColorBlendAttachmentState colorBlendAttachment
  {
    .blendEnable = true,
    .srcColorBlendFactor = GFX::BlendFactor::ONE,
    .dstColorBlendFactor = GFX::BlendFactor::ZERO,
    .colorBlendOp = GFX::BlendOp::ADD,
    .srcAlphaBlendFactor = GFX::BlendFactor::ONE,
    .dstAlphaBlendFactor = GFX::BlendFactor::ZERO,
    .alphaBlendOp = GFX::BlendOp::ADD,
    .colorWriteMask = GFX::ColorComponentFlag::RGBA_BITS
  };
  GFX::ColorBlendState colorBlend
  {
    .logicOpEnable = false,
    .logicOp{},
    .attachments = { &colorBlendAttachment, 1 },
    .blendConstants = {},
  };

  GFX::GraphicsPipelineInfo pipelineInfo
  {
    .shaderProgram = shader,
    .inputAssemblyState = inputAssembly,
    .vertexInputState = vertexInput,
    .rasterizationState = rasterization,
    .depthStencilState = depthStencil,
    .colorBlendState = colorBlend
  };

  auto pipeline = GFX::CompileGraphicsPipeline(pipelineInfo);
  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

GFX::GraphicsPipeline CreateShadowPipeline()
{
  GLuint shader = Utility::CompileVertexFragmentProgram(
    Utility::LoadFile("shaders/SceneDeferred.vert.glsl"),
    Utility::LoadFile("shaders/RSMScene.frag.glsl"));

  GFX::InputAssemblyState inputAssembly
  {
    .topology = GFX::PrimitiveTopology::TRIANGLE_LIST,
    .primitiveRestartEnable = false,
  };

  auto inputDescs = GetSceneInputBindingDescs();
  GFX::VertexInputState vertexInput{ inputDescs };

  auto rasterization = GetDefaultRasterizationState();
  rasterization.depthBiasEnable = true;
  rasterization.depthBiasConstantFactor = 0;
  rasterization.depthBiasSlopeFactor = 2;

  GFX::DepthStencilState depthStencil
  {
    .depthTestEnable = true,
    .depthWriteEnable = true,
    .depthCompareOp = GFX::CompareOp::LESS,
  };

  GFX::ColorBlendAttachmentState colorBlendAttachment
  {
    .blendEnable = false,
    .srcColorBlendFactor = GFX::BlendFactor::ONE,
    .dstColorBlendFactor = GFX::BlendFactor::ZERO,
    .colorBlendOp = GFX::BlendOp::ADD,
    .srcAlphaBlendFactor = GFX::BlendFactor::ONE,
    .dstAlphaBlendFactor = GFX::BlendFactor::ZERO,
    .alphaBlendOp = GFX::BlendOp::ADD,
    .colorWriteMask = GFX::ColorComponentFlag::RGBA_BITS
  };
  GFX::ColorBlendState colorBlend
  {
    .logicOpEnable = false,
    .logicOp{},
    .attachments = { &colorBlendAttachment, 1 },
    .blendConstants = {},
  };

  GFX::GraphicsPipelineInfo pipelineInfo
  {
    .shaderProgram = shader,
    .inputAssemblyState = inputAssembly,
    .vertexInputState = vertexInput,
    .rasterizationState = rasterization,
    .depthStencilState = depthStencil,
    .colorBlendState = colorBlend
  };

  auto pipeline = GFX::CompileGraphicsPipeline(pipelineInfo);
  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

GFX::GraphicsPipeline CreateShadingPipeline()
{
  GLuint shader = Utility::CompileVertexFragmentProgram(
    Utility::LoadFile("shaders/FullScreenTri.vert.glsl"),
    Utility::LoadFile("shaders/ShadeDeferred.frag.glsl"));

  GFX::InputAssemblyState inputAssembly
  {
    .topology = GFX::PrimitiveTopology::TRIANGLE_LIST,
    .primitiveRestartEnable = false,
  };

  GFX::VertexInputState vertexInput{};

  auto rasterization = GetDefaultRasterizationState();
  rasterization.cullMode = GFX::CullMode::NONE;

  GFX::DepthStencilState depthStencil
  {
    .depthTestEnable = false,
    .depthWriteEnable = false,
  };

  GFX::ColorBlendAttachmentState colorBlendAttachment
  {
    .blendEnable = false,
    .srcColorBlendFactor = GFX::BlendFactor::ONE,
    .dstColorBlendFactor = GFX::BlendFactor::ZERO,
    .colorBlendOp = GFX::BlendOp::ADD,
    .srcAlphaBlendFactor = GFX::BlendFactor::ONE,
    .dstAlphaBlendFactor = GFX::BlendFactor::ZERO,
    .alphaBlendOp = GFX::BlendOp::ADD,
    .colorWriteMask = GFX::ColorComponentFlag::RGBA_BITS
  };
  GFX::ColorBlendState colorBlend
  {
    .logicOpEnable = false,
    .logicOp{},
    .attachments = { &colorBlendAttachment, 1 },
    .blendConstants = {},
  };

  GFX::GraphicsPipelineInfo pipelineInfo
  {
    .shaderProgram = shader,
    .inputAssemblyState = inputAssembly,
    .vertexInputState = vertexInput,
    .rasterizationState = rasterization,
    .depthStencilState = depthStencil,
    .colorBlendState = colorBlend
  };

  auto pipeline = GFX::CompileGraphicsPipeline(pipelineInfo);
  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

GFX::ComputePipeline CreateRSMIndirectPipeline()
{
  GLuint shader = Utility::CompileComputeProgram(
    Utility::LoadFile("shaders/RSMIndirect.comp.glsl"));
  auto pipeline = GFX::CompileComputePipeline({ shader });
  if (!pipeline)
    throw std::exception("Invalid pipeline");
  return *pipeline;
}

void CursorPosCallback(GLFWwindow* window, double currentCursorX, double currentCursorY)
{
  static bool firstFrame = true;
  if (firstFrame)
  {
    gPreviousCursorX = currentCursorX;
    gPreviousCursorY = currentCursorY;
    firstFrame = false;
  }

  gCursorOffsetX = currentCursorX - gPreviousCursorX;
  gCursorOffsetY = gPreviousCursorY - currentCursorY;
  gPreviousCursorX = currentCursorX;
  gPreviousCursorY = currentCursorY;
}

void RenderScene()
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "Deferred Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glEnable(GL_FRAMEBUFFER_SRGB);

  GFX::Viewport mainViewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { gWindowWidth, gWindowHeight }
    },
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  GFX::Viewport rsmViewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { gShadowmapWidth, gShadowmapHeight }
    },
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  GFX::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = &mainViewport,
    .clearColorOnLoad = false,
    .clearColorValue = GFX::ClearColorValue {.f = { .0, .0, .0, 1.0 }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  // create gbuffer textures and render info
  auto gcolorTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R8G8B8A8_UNORM);
  auto gnormalTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R16G16B16_SNORM);
  auto gdepthTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::D32_UNORM);
  auto gcolorTexView = gcolorTex->View();
  auto gnormalTexView = gnormalTex->View();
  auto gdepthTexView = gdepthTex->View();
  GFX::RenderAttachment gcolorAttachment
  {
    .textureView = &gcolorTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ .1, .3, .5, 0 } } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment gnormalAttachment
  {
    .textureView = &gnormalTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  GFX::RenderAttachment gdepthAttachment
  {
    .textureView = &gdepthTexView.value(),
    .clearValue = GFX::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment cgAttachments[] = { gcolorAttachment, gnormalAttachment };
  GFX::RenderInfo gbufferRenderInfo
  {
    .viewport = &mainViewport,
    .colorAttachments = cgAttachments,
    .depthAttachment = &gdepthAttachment,
    .stencilAttachment = nullptr
  };

  // create RSM textures and render info
  auto rfluxTex = GFX::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, GFX::Format::R11G11B10_FLOAT);
  auto rnormalTex = GFX::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, GFX::Format::R16G16B16_SNORM);
  auto rdepthTex = GFX::CreateTexture2D({ gShadowmapWidth, gShadowmapHeight }, GFX::Format::D16_UNORM);
  auto rfluxTexView = rfluxTex->View();
  auto rnormalTexView = rnormalTex->View();
  auto rdepthTexView = rdepthTex->View();
  GFX::RenderAttachment rcolorAttachment
  {
    .textureView = &rfluxTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  GFX::RenderAttachment rnormalAttachment
  {
    .textureView = &rnormalTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = false
  };
  GFX::RenderAttachment rdepthAttachment
  {
    .textureView = &rdepthTexView.value(),
    .clearValue = GFX::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment crAttachments[] = { rcolorAttachment, rnormalAttachment };
  GFX::RenderInfo rsmRenderInfo
  {
    .viewport = &rsmViewport,
    .colorAttachments = crAttachments,
    .depthAttachment = &rdepthAttachment,
    .stencilAttachment = nullptr
  };

  std::optional<GFX::Texture> indirectLightingTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R16G16B16A16_FLOAT);
  std::optional<GFX::TextureView> indirectLightingTexView = indirectLightingTex->View();
  
  auto view = glm::mat4(1);
  auto proj = glm::perspective(glm::radians(70.f), gWindowWidth / (float)gWindowHeight, 0.1f, 100.f);

  std::vector<ObjectUniforms> objectUniforms;
  // translation, scale, color tuples
  std::tuple<glm::vec3, glm::vec3, glm::vec3> objects[]{
    { { 0, .5, -1 },   { 3, 1, 1 },      { .5, .5, .5 } },
    { { -1, .5, 0 },   { 1, 1, 1 },      { .1, .1, .9 } },
    { { 1, .5, 0 },    { 1, 1, 1 },      { .1, .1, .9 } },
    { { 0, -.5, -.5 }, { 3, 1, 2 },      { .5, .5, .5 } },
    { { 0, 1.5, -.5 }, { 3, 1, 2 },      { .2, .7, .2 } },
    { { 0, .25, 0 },   { 0.25, .5, .25 }, { .5, .1, .1 } },
    //{ { -.25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { .25, .25, 0 },   { .01, .5, .7 }, { .5, .1, .1 } },
    //{ { 0, .25, -.25 },   { .7, .5, .01 }, { .5, .1, .1 } },
    //{ { 0, .25, .25 },   { .7, .5, .01 }, { .5, .1, .1 } },
  };
  for (const auto& [translation, scale, color] : objects)
  {
    glm::mat4 model{ 1 };
    model = glm::translate(model, translation);
    model = glm::scale(model, scale);
    objectUniforms.push_back({ model, glm::vec4{ color, 0.0f } });
  }

  ShadingUniforms shadingUniforms
  {
    .sunDir = glm::normalize(glm::vec4{ -.1, -.3, -.6, 0 }),
    .sunStrength = glm::vec4{ 2, 2, 2, 0 },
  };

  RSMUniforms rsmUniforms
  {
    .targetDim = { gWindowWidth, gWindowHeight },
    .rMax = gRMax,
    .samples = gRSMSamples,
  };

  GlobalUniforms globalUniforms;

  auto vertexBuffer = GFX::Buffer::Create(gCubeVertices);
  auto indexBuffer = GFX::Buffer::Create(gCubeIndices);
  auto objectBuffer = GFX::Buffer::Create(std::span(objectUniforms), GFX::BufferFlag::DYNAMIC_STORAGE);
  auto globalUniformsBuffer = GFX::Buffer::Create(sizeof(globalUniforms), GFX::BufferFlag::DYNAMIC_STORAGE);
  auto shadingUniformsBuffer = GFX::Buffer::Create(shadingUniforms, GFX::BufferFlag::DYNAMIC_STORAGE);
  auto rsmUniformBuffer = GFX::Buffer::Create(rsmUniforms, GFX::BufferFlag::DYNAMIC_STORAGE);

  GFX::SamplerState ss;
  ss.asBitField.minFilter = GFX::Filter::NEAREST;
  ss.asBitField.magFilter = GFX::Filter::NEAREST;
  ss.asBitField.addressModeU = GFX::AddressMode::REPEAT;
  ss.asBitField.addressModeV = GFX::AddressMode::REPEAT;
  auto nearestSampler = GFX::TextureSampler::Create(ss);

  ss.asBitField.minFilter = GFX::Filter::LINEAR;
  ss.asBitField.magFilter = GFX::Filter::LINEAR;
  ss.asBitField.borderColor = GFX::BorderColor::FLOAT_TRANSPARENT_BLACK;
  ss.asBitField.addressModeU = GFX::AddressMode::CLAMP_TO_BORDER;
  ss.asBitField.addressModeV = GFX::AddressMode::CLAMP_TO_BORDER;
  auto rsmColorSampler = GFX::TextureSampler::Create(ss);

  ss.asBitField.minFilter = GFX::Filter::NEAREST;
  ss.asBitField.magFilter = GFX::Filter::NEAREST;
  ss.asBitField.borderColor = GFX::BorderColor::FLOAT_TRANSPARENT_BLACK;
  ss.asBitField.addressModeU = GFX::AddressMode::CLAMP_TO_BORDER;
  ss.asBitField.addressModeV = GFX::AddressMode::CLAMP_TO_BORDER;
  auto rsmDepthSampler = GFX::TextureSampler::Create(ss);

  ss.asBitField.compareEnable = true;
  ss.asBitField.compareOp = GFX::CompareOp::LESS;
  ss.asBitField.minFilter = GFX::Filter::LINEAR;
  ss.asBitField.magFilter = GFX::Filter::LINEAR;
  auto rsmShadowSampler = GFX::TextureSampler::Create(ss);

  GFX::GraphicsPipeline scenePipeline = CreateScenePipeline();
  GFX::GraphicsPipeline rsmScenePipeline = CreateShadowPipeline();
  GFX::GraphicsPipeline shadingPipeline = CreateShadingPipeline();
  GFX::ComputePipeline rsmIndirectPipeline = CreateRSMIndirectPipeline();

  View camera;
  camera.position = { 0, .5, 1 };
  camera.yaw = -glm::half_pi<float>();

  float prevFrame = glfwGetTime();
  while (!glfwWindowShouldClose(window))
  {
    float curFrame = glfwGetTime();
    float dt = curFrame - prevFrame;
    prevFrame = curFrame;

    gCursorOffsetX = 0;
    gCursorOffsetY = 0;
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    const glm::vec3 forward = camera.GetForwardDir();
    const glm::vec3 up = { 0, 1, 0 };
    const glm::vec3 right = glm::normalize(glm::cross(forward, up));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      camera.position += forward * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      camera.position -= forward * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      camera.position += right * dt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      camera.position -= right * dt;
    camera.yaw += gCursorOffsetX * gSensitivity;
    camera.pitch += gCursorOffsetY * gSensitivity;
    camera.pitch = glm::clamp(camera.pitch, -glm::half_pi<float>() + 1e-4f, glm::half_pi<float>() - 1e-4f);

    //objectUniforms[5].model = glm::rotate(glm::mat4(1), dt, {0, 1, 0}) * objectUniforms[5].model;
    //objectBuffer->SubData(std::span(objectUniforms), 0);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
      rsmUniforms.rMax -= .15 * dt;
      printf("rMax: %f\n", rsmUniforms.rMax);
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
      rsmUniforms.rMax += .15 * dt;
      printf("rMax: %f\n", rsmUniforms.rMax);
    }
    rsmUniforms.rMax = glm::clamp(rsmUniforms.rMax, 0.02f, 0.3f);

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ 1, 0, 0 }) * shadingUniforms.sunDir;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
    {
      shadingUniforms.sunDir = glm::rotate(glm::quarter_pi<float>() * dt, glm::vec3{ -1, 0, 0 }) * shadingUniforms.sunDir;
    }

    glm::mat4 viewProj = proj * camera.GetViewMatrix();
    globalUniformsBuffer->SubData(viewProj, 0);

    glm::vec3 eye = glm::vec3{ shadingUniforms.sunDir * -5.f };
    float eyeWidth = 2.5f;
    //shadingUniforms.viewPos = glm::vec4(camera.position, 0);
    shadingUniforms.sunViewProj =
      glm::ortho(-eyeWidth, eyeWidth, -eyeWidth, eyeWidth, .1f, 10.f) *
      glm::lookAt(eye, glm::vec3(0), glm::vec3{ 0, 1, 0 });
    shadingUniformsBuffer->SubData(shadingUniforms, 0);

    // geometry buffer pass
    GFX::BeginRendering(gbufferRenderInfo);
    {
      GFX::ScopedDebugMarker marker("Geometry");
      GFX::Cmd::BindGraphicsPipeline(scenePipeline);
      GFX::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
      GFX::Cmd::BindIndexBuffer(*indexBuffer, GFX::IndexType::UNSIGNED_SHORT);
      GFX::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      GFX::Cmd::BindStorageBuffer(1, *objectBuffer, 0, objectBuffer->Size());
      GFX::Cmd::DrawIndexed(gCubeIndices.size(), objectUniforms.size(), 0, 0, 0);
    }
    GFX::EndRendering();

    globalUniformsBuffer->SubData(shadingUniforms.sunViewProj, 0);

    // shadow map (RSM) scene pass
    GFX::BeginRendering(rsmRenderInfo);
    {
      GFX::ScopedDebugMarker marker("RSM Scene");
      GFX::Cmd::BindGraphicsPipeline(rsmScenePipeline);
      GFX::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
      GFX::Cmd::BindIndexBuffer(*indexBuffer, GFX::IndexType::UNSIGNED_SHORT);
      GFX::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      GFX::Cmd::BindUniformBuffer(1, *shadingUniformsBuffer, 0, shadingUniformsBuffer->Size());
      GFX::Cmd::BindStorageBuffer(1, *objectBuffer, 0, objectBuffer->Size());
      GFX::Cmd::DrawIndexed(gCubeIndices.size(), objectUniforms.size(), 0, 0, 0);
    }
    GFX::EndRendering();

    globalUniformsBuffer->SubData(viewProj, 0);
    globalUniformsBuffer->SubData(glm::inverse(viewProj), sizeof(glm::mat4));

    rsmUniforms.sunViewProj = shadingUniforms.sunViewProj;
    rsmUniforms.invSunViewProj = glm::inverse(rsmUniforms.sunViewProj);
    rsmUniformBuffer->SubData(rsmUniforms, 0);

    // RSM indirect illumination calculation pass
    GFX::BeginCompute();
    {
      // uncomment to benchmark
      //static GFX::TimerQueryAsync timer(5);
      //if (auto t = timer.Elapsed_ns())
      //{
      //  printf("Indirect Illumination: %f ms\n", *t / 10e5);
      //}
      //GFX::TimerScoped scopedTimer(timer);
      GFX::ScopedDebugMarker marker("Indirect Illumination");
      GFX::Cmd::BindComputePipeline(rsmIndirectPipeline);
      GFX::Cmd::BindSampledImage(0, *indirectLightingTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(1, *gcolorTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(2, *gnormalTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(3, *gdepthTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(4, *rfluxTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(5, *rnormalTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(6, *rdepthTexView, *nearestSampler);
      GFX::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      GFX::Cmd::BindUniformBuffer(1, *rsmUniformBuffer, 0, rsmUniformBuffer->Size());
      GFX::Cmd::BindImage(0, *indirectLightingTexView, 0);

      const int localSize = 8;
      const int numGroupsX = (rsmUniforms.targetDim.x / 2 + localSize - 1) / localSize;
      const int numGroupsY = (rsmUniforms.targetDim.y / 2 + localSize - 1) / localSize;

      uint32_t currentPass = 0;
      rsmUniformBuffer->SubData(currentPass, offsetof(RSMUniforms, currentPass));
      GFX::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      GFX::Cmd::MemoryBarrier(GFX::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 1;
      rsmUniformBuffer->SubData(currentPass, offsetof(RSMUniforms, currentPass));
      GFX::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      GFX::Cmd::MemoryBarrier(GFX::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 2;
      rsmUniformBuffer->SubData(currentPass, offsetof(RSMUniforms, currentPass));
      GFX::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      GFX::Cmd::MemoryBarrier(GFX::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);

      currentPass = 3;
      rsmUniformBuffer->SubData(currentPass, offsetof(RSMUniforms, currentPass));
      GFX::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      GFX::Cmd::MemoryBarrier(GFX::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    }
    GFX::EndCompute();

    // shading pass (full screen tri)
    GFX::BeginSwapchainRendering(swapchainRenderingInfo);
    {
      GFX::ScopedDebugMarker marker("Shading");
      GFX::Cmd::BindGraphicsPipeline(shadingPipeline);
      GFX::Cmd::BindSampledImage(0, *gcolorTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(1, *gnormalTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(2, *gdepthTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(3, *indirectLightingTexView, *nearestSampler);
      GFX::Cmd::BindSampledImage(4, *rdepthTexView, *rsmShadowSampler);
      GFX::Cmd::BindUniformBuffer(0, *globalUniformsBuffer, 0, globalUniformsBuffer->Size());
      GFX::Cmd::BindUniformBuffer(1, *shadingUniformsBuffer, 0, shadingUniformsBuffer->Size());
      GFX::Cmd::Draw(3, 1, 0, 0);
    }
    GFX::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();
}

int main()
{
  try
  {
    RenderScene();
  }
  catch (std::exception e)
  {
    printf("Error: %s", e.what());
    throw;
  }
  catch (...)
  {
    printf("Unknown error");
    throw;
  }

  return 0;
}