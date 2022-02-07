#include "common.h"

#include <array>

#include <gsdf/BasicTypes.h>
#include <gsdf/Fence.h>
#include <gsdf/Rendering.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

struct Uniforms
{
  glm::mat4 model;
  glm::mat4 viewProj;
};

////////////////////////////////////// Globals
constexpr int gWindowWidth = 1280;
constexpr int gWindowHeight = 720;

std::array<glm::vec3, 3> gTriVertices
{
  glm::vec3{-0.f, -0.f, -1},
  glm::vec3{1.f, -1.f, -1},
  glm::vec3{1.f, 1.f, -1}
};
std::array<glm::u8vec3, 3> gTriColors
{
  glm::u8vec3{255, 0, 0},
  glm::u8vec3{0, 255, 0},
  glm::u8vec3{0, 0, 255}
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
    .offset = 0,
  };
  GFX::VertexInputBindingDescription descColor
  {
    .location = 1,
    .binding = 1,
    .format = GFX::Format::R8G8B8_UNORM,
    .offset = 0,
  };
  GFX::VertexInputBindingDescription inputDescs[] = { descPos, descColor };
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
    .depthTestEnable = false,
    .depthWriteEnable = false,
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

int main()
{
  GLFWwindow* window = Utility::CreateWindow({
    .name = "Deferred Example",
    .maximize = false,
    .decorate = true,
    .width = gWindowWidth,
    .height = gWindowHeight });
  Utility::InitOpenGL();

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
    .clearColorOnLoad = true,
    .clearColorValue = GFX::ClearColorValue {.f = { .2, .0, .2, 1 }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  auto gcolorTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R8G8B8A8_UNORM);
  auto gnormalTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::R16G16B16_SNORM);
  auto gdepthTex = GFX::CreateTexture2D({ gWindowWidth, gWindowHeight }, GFX::Format::D32_UNORM);
  auto gcolorTexView = gcolorTex->View();
  auto gnormalTexView = gnormalTex->View();
  auto gdepthTexView = gdepthTex->View();
  GFX::RenderAttachment colorAttachment
  {
    .textureView = &gcolorTexView.value(),
    .clearValue = GFX::ClearValue{ .color{ .f{ 0, 1, 0, 0 } } },
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
    .clearValue = GFX::ClearValue{.depthStencil{ .depth = 0.0f } },
    .clearOnLoad = true
  };
  GFX::RenderAttachment cAttachments[2] = { colorAttachment, normalAttachment };
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

  auto vertexPosBuffer = GFX::Buffer::Create(gTriVertices);
  auto vertexColorBuffer = GFX::Buffer::Create(gTriColors);
  auto uniformBuffer = GFX::Buffer::Create(uniforms, GFX::BufferFlag::DYNAMIC_STORAGE);

  auto sampler = GFX::TextureSampler::Create({});

  GFX::GraphicsPipeline scenePipeline = CreateScenePipeline();
  GFX::GraphicsPipeline shadingPipeline = CreateShadingPipeline();

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    uniforms.model = glm::translate(glm::vec3{ 0.f, sinf(glfwGetTime()) / 5.f, cosf(glfwGetTime()) / 2.f });
    uniformBuffer->SubData(uniforms, 0);

    GFX::BeginRendering(gbufferRenderInfo);
    GFX::Cmd::BindGraphicsPipeline(scenePipeline);
    GFX::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, sizeof(glm::vec3));
    GFX::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, sizeof(glm::u8vec3));
    GFX::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    GFX::Cmd::Draw(3, 1, 0, 0);
    GFX::EndRendering();

    GFX::BeginSwapchainRendering(swapchainRenderingInfo);
    GFX::Cmd::BindGraphicsPipeline(shadingPipeline);
    GFX::Cmd::BindSampledImage(0, *gcolorTexView, *sampler);
    GFX::Cmd::Draw(3, 1, 0, 0);
    GFX::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();

  return 0;
}