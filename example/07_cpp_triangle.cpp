#include "common/Application.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Exception.h>

#include <array>

#include <fstream>
#include <sstream>
#include <string>

/* 07_cpp_triangle
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

// Fragment shader written in freestanding C++20
const char* gFragmentSourceCpp = R"(
#include <shady.h>

location(0) output vcc::native_vec4 o_color;

location(0) input vcc::native_vec3 v_color;

[[nodiscard]] auto to_vec4(auto vec) -> vcc::native_vec4
{
  return {vec.x, vec.y, vec.z, 1.0f};
}

extern "C"
fragment_shader
void main()
{
  o_color = to_vec4(v_color);
}
)";

class CppTriangleApplication final : public Application
{
public:
  CppTriangleApplication(const Application::CreateInfo& createInfo);
  
  ~CppTriangleApplication() = default;

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
  auto descPos = Fwog::VertexInputBindingDescription{
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = 0,
  };
  auto descColor = Fwog::VertexInputBindingDescription{
    .location = 1,
    .binding = 1,
    .format = Fwog::Format::R8G8B8_UNORM,
    .offset = 0,
  };
  auto inputDescs = {descPos, descColor};
  
  auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, gVertexSource, "Triangle VS");
  const auto fragmentSourceInfo = Fwog::ShaderSourceInfo{
    .language = Fwog::SourceLanguage::CPP,
    .source = gFragmentSourceCpp,
  };
  auto fragmentShader = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, fragmentSourceInfo, "Triangle FS");
                                       
  return Fwog::GraphicsPipeline{{
    .name = "Triangle Pipeline",
    .vertexShader = &vertexShader,
    .fragmentShader = &fragmentShader,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_LIST},
    .vertexInputState = {inputDescs},
  }};
}

CppTriangleApplication::CppTriangleApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    vertexPosBuffer(triPositions),
    vertexColorBuffer(triColors),
    pipeline(CreatePipeline())
{
}

// OnRender is automatically called every frame when we run the app
void CppTriangleApplication::OnRender([[maybe_unused]] double dt)
{
  Fwog::RenderToSwapchain(
    Fwog::SwapchainRenderInfo{
      .name = "Render Triangle",
      .viewport = Fwog::Viewport{.drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}}},
      .colorLoadOp = Fwog::AttachmentLoadOp::CLEAR,
      .clearColorValue = {.2f, .0f, .2f, 1.0f},
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(pipeline);
      
      Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
      Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));
      
      Fwog::Cmd::Draw(3, 1, 0, 0);
    });
}

int main()
{
  auto appInfo = Application::CreateInfo{
    .name = "Hello Triangle",
    .maximize = false,
    .decorate = true,
    .vsync = true,
  };
  auto app = CppTriangleApplication(appInfo);

  app.Run();

  return 0;
}
