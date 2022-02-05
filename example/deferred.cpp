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

const char* gVertexSource = R"(
#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;

layout(location = 0) out vec3 v_color;

layout(binding = 0, std140) uniform Uniforms
{
  mat4 model;
  mat4 viewProj;
};

void main()
{
  v_color = a_color;
  gl_Position = viewProj * model * vec4(a_pos, 1.0);
}
)";

const char* gFragmentSource = R"(
#version 460 core

layout(location = 0) out vec4 o_color;

layout(location = 0) in vec3 v_color;

void main()
{
  o_color = vec4(v_color, 1.0);
}
)";

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

int main()
{
  GLFWwindow* window = Utility::CreateWindow({ .maximize = false, .decorate = true, .width = gWindowWidth, .height = gWindowHeight });
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
    .maxDepth = 0.0f,
  };

  GFX::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = &viewport,
    .clearColorOnLoad = true,
    .clearColorValue = GFX::ClearColorValue {.f = { .2, .0, .2, 1 }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  auto view = glm::mat4(1);
  auto proj = glm::perspective(glm::radians(70.f), gWindowWidth / (float)gWindowHeight, 0.1f, 100.f);

  Uniforms uniforms;
  uniforms.model = glm::mat4(1);
  uniforms.viewProj = proj * view;

  GLuint shader = Utility::CompileVertexFragmentProgram(gVertexSource, gFragmentSource);
  auto vertexPosBuffer = GFX::Buffer::Create<glm::vec3>(gTriVertices);
  auto vertexColorBuffer = GFX::Buffer::Create<glm::u8vec3>(gTriColors);
  auto uniformBuffer = GFX::Buffer::Create<Uniforms>({ &uniforms, 1 }, GFX::BufferFlag::DYNAMIC_STORAGE);

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

  GFX::GraphicsPipelineInfo pipeline
  {
    .shaderProgram = shader,
    .inputAssemblyState = inputAssembly,
    .vertexInputState = vertexInput,
    .rasterizationState = rasterization,
    .depthStencilState = depthStencil,
    .colorBlendState = colorBlend
  };

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    uniforms.model = glm::translate(glm::vec3{ 0.f, sinf(glfwGetTime()) / 5.f, cosf(glfwGetTime()) / 2.f });
    uniformBuffer->SubData<Uniforms>({ &uniforms, 1 }, 0);

    GFX::BeginSwapchainRendering(swapchainRenderingInfo);
    GFX::Cmd::BindGraphicsPipeline(pipeline);
    GFX::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, sizeof(glm::vec3));
    GFX::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, sizeof(glm::u8vec3));
    GFX::Cmd::BindUniformBuffer(0, *uniformBuffer, 0, uniformBuffer->Size());
    GFX::Cmd::Draw(3, 1, 0, 0);
    GFX::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();

  return 0;
}