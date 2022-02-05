#include "common.h"

#include <array>

#include <gsdf/BasicTypes.h>
#include <gsdf/Fence.h>
#include <gsdf/Rendering.h>

////////////////////////////////////// Globals
const char* gVertexSource = R"(
#version 460 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec3 a_color;

layout(location = 0) out vec3 v_color;

void main()
{
  v_color = a_color;
  gl_Position = vec4(a_pos, 0.0, 1.0);
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

std::array<float, 6> gTriVertices = { -0, -0, 1, -1, 1, 1 };
std::array<uint8_t, 9> gTriColors = { 255, 0, 0, 0, 255, 0, 0, 0, 255 };

int main()
{
  GLFWwindow* window = Utility::CreateWindow({ .maximize = false, .decorate = true, .width = 1280, .height = 720 });
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

  GLuint shader = Utility::CompileVertexFragmentProgram(gVertexSource, gFragmentSource);
  auto vertexPosBuffer = GFX::Buffer::Create(std::span<const float>(gTriVertices));
  auto vertexColorBuffer = GFX::Buffer::Create(std::span<const uint8_t>(gTriColors));

  GFX::InputAssemblyState inputAssembly
  {
    .topology = GFX::PrimitiveTopology::TRIANGLE_LIST,
    .primitiveRestartEnable = false,
  };

  GFX::VertexInputBindingDescription descPos
  {
    .location = 0,
    .binding = 0,
    .format = GFX::Format::R32G32_FLOAT,
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

    GFX::BeginSwapchainRendering(swapchainRenderingInfo);
    GFX::Cmd::BindGraphicsPipeline(pipeline);
    GFX::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, 2 * sizeof(float));
    GFX::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, 3 * sizeof(uint8_t));
    GFX::Cmd::Draw(3, 1, 0, 0);
    GFX::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();

  return 0;
}