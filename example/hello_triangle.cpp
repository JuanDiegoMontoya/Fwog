#include "common/common.h"

#include <array>
#include <iostream>

#include <Fwog/BasicTypes.h>
#include <Fwog/Rendering.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Buffer.h>
#include <Fwog/Shader.h>
#include <Fwog/Exception.h>

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

Fwog::GraphicsPipeline CreatePipeline()
{
  Fwog::VertexInputBindingDescription descPos
  {
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = 0,
  };
  Fwog::VertexInputBindingDescription descColor
  {
    .location = 1,
    .binding = 1,
    .format = Fwog::Format::R8G8B8_UNORM,
    .offset = 0,
  };
  Fwog::VertexInputBindingDescription inputDescs[] = { descPos, descColor };

  auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, gVertexSource);
  auto fragmentShader = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, gFragmentSource);

  auto pipeline = Fwog::CompileGraphicsPipeline(Fwog::GraphicsPipelineInfo
    {
      .vertexShader = &vertexShader,
      .fragmentShader = &fragmentShader,
      .vertexInputState = { inputDescs },
      .depthState = { .depthTestEnable = false, .depthWriteEnable = false }
    });

  return pipeline;
}

int main()
{
  GLFWwindow* window = Utility::CreateWindow({ .name = "Hello Triangle", .maximize = false, .decorate = true, .width = 1280, .height = 720});
  Utility::InitOpenGL();

  glEnable(GL_FRAMEBUFFER_SRGB);

  Fwog::Viewport viewport
  {
    .drawRect
    {
      .offset = { 0, 0 },
      .extent = { 1280, 720 }
    },
    .minDepth = 0.0f,
    .maxDepth = 0.0f,
  };

  Fwog::SwapchainRenderInfo swapchainRenderingInfo
  {
    .viewport = viewport,
    .clearColorOnLoad = true,
    .clearColorValue = Fwog::ClearColorValue {.f = { .2f, .0f, .2f, 1.0f }},
    .clearDepthOnLoad = false,
    .clearStencilOnLoad = false,
  };

  auto vertexPosBuffer = Fwog::Buffer(gTriVertices);
  auto vertexColorBuffer = Fwog::Buffer(gTriColors);

  Fwog::GraphicsPipeline pipeline = CreatePipeline();

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
    Fwog::Cmd::BindGraphicsPipeline(pipeline);
    Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
    Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));
    Fwog::Cmd::Draw(3, 1, 0, 0);
    Fwog::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();

  return 0;
}