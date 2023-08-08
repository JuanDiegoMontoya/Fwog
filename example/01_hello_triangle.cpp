#include "common/Application.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>

#include <array>

/* 01_hello_triangle
 *
 * This example renders a simple triangle with Fwog.
 *
 * Shown:
 * + Creating vertex buffers
 * + Specifying vertex attributes
 * + Loading shaders
 * + Creating a graphics pipeline
 * + Rendering to the screen
 *
 * All of the examples use a common framework to reduce code duplication between examples.
 * It abstracts away boring things like creating a window and loading OpenGL function pointers.
 * It also provides a main loop, inside of which our rendering function is called.
 */

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

class TriangleApplication final : public Application
{
public:
  TriangleApplication(const Application::CreateInfo& createInfo);

  // All Fwog resources will be automatically cleaned up as they go out of scope.
  ~TriangleApplication() = default;

  void OnRender(double dt) override;

private:
  static constexpr std::array<float, 6> triPositions = {-0, -0, 1, -1, 1, 1};
  static constexpr std::array<uint8_t, 9> triColors = {255, 0, 0, 0, 255, 0, 0, 0, 255};

  Fwog::Buffer vertexPosBuffer;
  Fwog::Buffer vertexColorBuffer;
  Fwog::GraphicsPipeline pipeline;
};

static Fwog::GraphicsPipeline CreatePipeline()
{
  // Specify our two vertex attributes: position and color.
  // Positions are 2x float, so we will use R32G32_FLOAT like we would in Vulkan.
  auto descPos = Fwog::VertexInputBindingDescription{
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = 0,
  };
  // Colors are 3x uint8, so we will use R8G8B8.
  // To treat them as normalized floats in [0, 1], we make sure it's a _UNORM format.
  // This means we do not need to specify whether the data is normalized like we would with glVertexAttribPointer.
  auto descColor = Fwog::VertexInputBindingDescription{
    .location = 1,
    .binding = 1,
    .format = Fwog::Format::R8G8B8_UNORM,
    .offset = 0,
  };
  // Create an initializer list or array (or anything implicitly convertable to std::span)
  // of our input binding descriptions to send to the pipeline.
  auto inputDescs = {descPos, descColor};

  // We compile our shaders here. Just provide the shader source string and you are good to go!
  // Note that the shaders are compiled here and throw ShaderCompilationException if they fail.
  // The compiler's error message will be stored in the exception.
  // In a real application we might handle these exceptions, but here we will let them propagate up.
  auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, gVertexSource, "Triangle VS");
  auto fragmentShader = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, gFragmentSource, "Triangle FS");

  // The graphics pipeline contains all the state necessary for rendering.
  // It is self-contained, immutable, and isolated from other pipelines' state (state leaking cannot happen).
  // Here we give it our two shaders and the input binding descriptions.
  // We could specify a lot more state if we wanted, but for this simple example the defaults will suffice.
  // Note that compiling a pipeline will link its non-null shaders together.
  // If linking fails, a PipelineCompilationException containing the linker error will be thrown.
  // Similar to before, we will let possible exceptions propagate up.
  return Fwog::GraphicsPipeline{{
    .name = "Triangle Pipeline",
    .vertexShader = &vertexShader,
    .fragmentShader = &fragmentShader,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_LIST},
    .vertexInputState = {inputDescs},
  }};
}

TriangleApplication::TriangleApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    // Load the triangle's vertices (3 * vec2 position, 3 * 8 bit colors).
    // The colors will be specified as a UNORM integer format so they are treated as [0, 1] floats in the shader.
    vertexPosBuffer(triPositions),
    vertexColorBuffer(triColors),
    pipeline(CreatePipeline())
{
}

// OnRender is automatically called every frame when we run the app
void TriangleApplication::OnRender([[maybe_unused]] double dt)
{
  // Before we are allowed to render anything, we must declare what we are rendering to.
  // In this case we are rendering straight to the screen, so we can use RenderToSwapchain.
  // We are also provided with an opportunity to clear any of the render targets here (by setting the load op to clear).
  // We will use it to clear the color buffer with a soothing dark magenta.
  Fwog::RenderToSwapchain(
    Fwog::SwapchainRenderInfo{
      .name = "Render Triangle",
      .viewport = Fwog::Viewport{.drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}}},
      .colorLoadOp = Fwog::AttachmentLoadOp::CLEAR,
      .clearColorValue = {.2f, .0f, .2f, 1.0f},
    },
    [&]
    {
      // Functions in Fwog::Cmd can only be called inside a rendering (Begin*Rendering) or compute scope (BeginCompute).
      // Pipelines must be bound before we can issue drawing-related calls.
      // This is where, under the hood, the actual GL program is bound and all the pipeline state is set.
      Fwog::Cmd::BindGraphicsPipeline(pipeline);

      // Vertex buffers are bound at draw time, similar to Vulkan or with glBindVertexBuffer.
      Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
      Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));

      // Let's draw 1 instance with 3 vertices.
      Fwog::Cmd::Draw(3, 1, 0, 0);
    });
}

int main()
{
  // Create our app with the specified settings and run it.
  auto appInfo = Application::CreateInfo{
    .name = "Hello Triangle",
    .maximize = false,
    .decorate = true,
    .vsync = true,
  };
  auto app = TriangleApplication(appInfo);

  app.Run();

  return 0;
}
