#include "common.h"

#include <array>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <gsdf/BasicTypes.h>
#include <gsdf/Fence.h>
#include <gsdf/Rendering.h>

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

struct Uniforms
{
  glm::mat4 model;
  glm::mat4 viewProj;
};

////////////////////////////////////// Globals
constexpr int gWindowWidth = 1280;
constexpr int gWindowHeight = 720;
float gPreviousCursorX = gWindowWidth / 2.0f;
float gPreviousCursorY = gWindowHeight / 2.0f;
float gCursorOffsetX = 0;
float gCursorOffsetY = 0;
float gSensitivity = 0.005f;

struct VertexColor
{
  glm::vec3 position;
  glm::u8vec3 color;
};

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

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
  GFX::VertexInputBindingDescription inputDescs[] = { descPos, descNormal, descUV };
  GFX::VertexInputState vertexInput{ inputDescs };

  GFX::RasterizationState rasterization
  {
    .depthClampEnable = false,
    .polygonMode = GFX::PolygonMode::FILL,
    .cullMode = GFX::CullMode::BACK,
    .frontFace = GFX::FrontFace::COUNTERCLOCKWISE,
    .depthBiasEnable = false,
    .lineWidth = 1.0f,
    .pointSize = 1.0f,
  };

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

  GFX::RasterizationState rasterization
  {
    .depthClampEnable = false,
    .polygonMode = GFX::PolygonMode::FILL,
    .cullMode = GFX::CullMode::NONE,
    .frontFace = GFX::FrontFace::COUNTERCLOCKWISE,
    .depthBiasEnable = false,
    .lineWidth = 1.0f,
    .pointSize = 1.0f,
  };

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

  GFX::Viewport viewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { 1280, 720 }
    },
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  GFX::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = &viewport,
    .clearColorOnLoad = false,
    .clearColorValue = GFX::ClearColorValue {.f = { .0, .0, .0, 1.0 }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  auto gcolorTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R8G8B8A8_UNORM);
  auto gnormalTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R16G16B16_SNORM);
  auto gdepthTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::D24_UNORM);
  auto gcolorTexView = gcolorTex->View();
  auto gnormalTexView = gnormalTex->View();
  auto gdepthTexView = gdepthTex->View();
  GFX::RenderAttachment colorAttachment
  {
    .textureView = &gcolorTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ 0, 1, 0, 0 } } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment normalAttachment
  {
    .textureView = &gnormalTexView.value(),
    .clearValue = GFX::ClearValue{.color{.f{ 0, 0, 0, 0 } } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment depthAttachment
  {
    .textureView = &gdepthTexView.value(),
    .clearValue = GFX::ClearValue{.depthStencil{.depth = 1.0f } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment cAttachments[] = { colorAttachment/*, normalAttachment*/ };
  GFX::RenderInfo gbufferRenderInfo
  {
    .viewport = &viewport,
    .colorAttachments = cAttachments,
    .depthAttachment = &depthAttachment,
    .stencilAttachment = nullptr
  };

  auto view = glm::mat4(1);
  auto proj = glm::perspective(glm::radians(70.f), gWindowWidth / (float)gWindowHeight, 0.1f, 100.f);

  Uniforms uniforms;
  uniforms.model = glm::mat4(1);
  uniforms.viewProj = proj * view;

  auto vertexBuffer = GFX::Buffer::Create(gCubeVertices);
  auto indexBuffer = GFX::Buffer::Create(gCubeIndices);
  auto uniformBuffer = GFX::Buffer::Create(uniforms, GFX::BufferFlag::DYNAMIC_STORAGE);

  auto sampler = GFX::TextureSampler::Create({});

  GFX::GraphicsPipeline scenePipeline = CreateScenePipeline();
  GFX::GraphicsPipeline shadingPipeline = CreateShadingPipeline();

  View camera;
  camera.position = { 0, 0, 1 };
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

    //uniforms.model = glm::translate(glm::vec3{ 0.f, sinf(glfwGetTime()) / 5.f, cosf(glfwGetTime()) / 2.f });
    uniforms.viewProj = proj * camera.GetViewMatrix();
    uniformBuffer->SubData(uniforms, 0);

    GFX::BeginRendering(gbufferRenderInfo);
    GFX::Cmd::BindGraphicsPipeline(scenePipeline);
    GFX::Cmd::BindVertexBuffer(0, *vertexBuffer, 0, sizeof(Vertex));
    GFX::Cmd::BindIndexBuffer(*indexBuffer, GFX::IndexType::UNSIGNED_SHORT);
    GFX::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    GFX::Cmd::DrawIndexed(gCubeIndices.size(), 1, 0, 0, 0);
    GFX::EndRendering();

    GFX::BeginSwapchainRendering(swapchainRenderingInfo);
    GFX::Cmd::BindGraphicsPipeline(shadingPipeline);
    GFX::Cmd::BindSampledImage(0, *gcolorTexView, *sampler);
    GFX::Cmd::Draw(3, 1, 0, 0);
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