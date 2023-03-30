#include "common/Application.h"

#include <array>

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>


#include <glm/glm.hpp>

/* Hello Square
 * My first Fwog application :D
 * Just Hello Triangle but I make it a square instead. Keeping it Shrimple c:
 *
 */


//Load this externally later once I got the square indices working becasue lazy

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




class SquareApplication final : public Application
{
public:
  SquareApplication(const Application::CreateInfo& createInfo);

  // All Fwog resources will be automatically cleaned up as they go out of scope.
  ~SquareApplication() = default;

  void OnRender(double dt) override;

private:
  //static constexpr std::array<float, 6> triPositions = {-0, -0, 1, -1, 1, 1};
  //static constexpr std::array<uint8_t, 9> triColors = {255, 0, 0, 0, 255, 0, 0, 0, 255};


   //Lets draw it with indices

  //Bottom Left = (-1, -1)
  //Bottom Right = (1, -1)
  //Top Right = (1, 1)
  //Top Left = (-1, 1)

  static constexpr uint32_t numIndices = 6;
  static constexpr uint32_t numColorInt = 12;
  static constexpr uint32_t numFloatPos = 8;


  //imagine using a model transformation. cant be me
  static constexpr float scaleSet = 0.5f;

  static constexpr std::array<float, 8> squarePos = {
    -1 * scaleSet, -1 * scaleSet, 1 * scaleSet, -1 * scaleSet, 1 * scaleSet, 1 * scaleSet, -1 * scaleSet, 1 * scaleSet};

   static constexpr std::array<uint8_t, 12> squareColors = {0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0};

  //counter clockwise as god intended
   static constexpr std::array<uint32_t, numIndices> squareIndices = {0, 1, 2, 2, 3, 0};

  
  Fwog::Buffer vertexPosBuffer;
  Fwog::Buffer vertexColorBuffer;
  
  
  Fwog::Buffer indexBuffer;

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

  auto inputDescs = {descPos, descColor};

  auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, gVertexSource);
  auto fragmentShader = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, gFragmentSource);

  return Fwog::GraphicsPipeline{{
    .vertexShader = &vertexShader,
    .fragmentShader = &fragmentShader,
    .vertexInputState = {inputDescs},
  }};
}

SquareApplication::SquareApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    // Need to incrase the vertices to 4x 2 float for the position and for color is 3x4 uint8
    // The colors will be specified as a UNORM integer format so they are treated as [0, 1] floats in the shader.
    vertexPosBuffer(squarePos),
    vertexColorBuffer(squareColors),
    indexBuffer(squareIndices),
    pipeline(CreatePipeline())
{

}

// OnRender is automatically called every frame when we run the app
void SquareApplication::OnRender([[maybe_unused]] double dt)
{
  // Before we are allowed to render anything, we must declare what we are rendering to.
  // In this case we are rendering straight to the screen, so we can use BeginSwapchainRendering.
  // We are also provided with an opportunity to clear any of the render targets here (by setting the load op to clear).
  // We will use it to clear the color buffer with a soothing dark magenta.
  Fwog::BeginSwapchainRendering(Fwog::SwapchainRenderInfo{
    .viewport = Fwog::Viewport{.drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}}},
    .colorLoadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearColorValue = {.2f, .0f, .2f, 1.0f},
  });

  // Functions in Fwog::Cmd can only be called inside a rendering (Begin*Rendering) or compute scope (BeginCompute).
  // Pipelines must be bound before we can issue drawing-related calls.
  // This is where, under the hood, the actual GL program is bound and all the pipeline state is set.
  Fwog::Cmd::BindGraphicsPipeline(pipeline);

  // Vertex buffers are bound at draw time, similar to Vulkan or with glBindVertexBuffer.
  Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
  Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));
  Fwog::Cmd::BindIndexBuffer(indexBuffer, Fwog::IndexType::UNSIGNED_INT);

  //Draw one instance but with Indexed 
  Fwog::Cmd::DrawIndexed(numIndices, 1, 0, 0, 0);

  // Let's draw 1 instance with 8 vertices.
  //Fwog::Cmd::Draw(8, 1, 0, 0);/

  // Ending the rendering scope means no more Cmds can be issued and no state will leak.
  // It is required to not already be in a rendering scope when starting a new one.
  Fwog::EndRendering();
}

int main()
{
  // Create our app with the specified settings and run it.
  auto appInfo = Application::CreateInfo{
    .name = "Hello Square (A Tess Original Production)",
    .maximize = false,
    .decorate = true,
    .vsync = true,
  };
  auto app = SquareApplication(appInfo);

  app.Run();

  return 0;
}
