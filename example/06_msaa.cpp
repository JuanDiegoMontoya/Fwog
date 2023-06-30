#include "common/Application.h"

#include <Fwog/BasicTypes.h>
#include <Fwog/Buffer.h>
#include <Fwog/Context.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>

#include <imgui.h>

#include <array>
#include <optional>

/* 06_msaa
 *
 * This example renders a spinning triangle with MSAA.
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

layout(binding = 0) uniform Uniforms { float time; };

void main()
{
  v_color = a_color;

  mat2 rot = mat2(
    cos(time), sin(time),
    -sin(time), cos(time)
  );

  gl_Position = vec4(rot * a_pos, 0.0, 1.0);
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

class MultisampleApplication final : public Application
{
public:
  MultisampleApplication(const Application::CreateInfo& createInfo);

  ~MultisampleApplication() = default;

  void OnWindowResize(uint32_t newWidth, uint32_t newHeight) override;

  void OnRender(double dt) override;

  void OnGui(double dt) override;

private:
  static constexpr std::array<float, 6> triPositions = {-0.5, -0.5, 0.5, -0.5, 0.0, 0.5};
  static constexpr std::array<uint8_t, 9> triColors = {255, 0, 0, 0, 255, 0, 0, 0, 255};

  Fwog::Buffer vertexPosBuffer;
  Fwog::Buffer vertexColorBuffer;
  Fwog::TypedBuffer<float> timeBuffer;
  Fwog::GraphicsPipeline pipeline;
  std::optional<Fwog::Texture> msColorTex;
  std::optional<Fwog::Texture> resolveColorTex;

  double timeAccum = 0.0;
  Fwog::SampleCount numSamples = Fwog::SampleCount::SAMPLES_8;
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

  auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, gVertexSource);
  auto fragmentShader = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, gFragmentSource);

  return Fwog::GraphicsPipeline{{
    .vertexShader = &vertexShader,
    .fragmentShader = &fragmentShader,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_LIST},
    .vertexInputState = {inputDescs},
  }};
}

MultisampleApplication::MultisampleApplication(const Application::CreateInfo& createInfo)
  : Application(createInfo),
    vertexPosBuffer(triPositions),
    vertexColorBuffer(triColors),
    timeBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
    pipeline(CreatePipeline())
{
  OnWindowResize(windowWidth, windowHeight);
}

void MultisampleApplication::OnWindowResize(uint32_t newWidth, uint32_t newHeight)
{
  msColorTex = Fwog::Texture({
    .imageType = Fwog::ImageType::TEX_2D_MULTISAMPLE,
    .format = Fwog::Format::R8G8B8A8_SRGB,
    .extent = {newWidth / 8, newHeight / 8},
    .mipLevels = 1,
    .arrayLayers = 1,
    .sampleCount = numSamples,
  });

  resolveColorTex = Fwog::Texture({
    .imageType = Fwog::ImageType::TEX_2D,
    .format = Fwog::Format::R8G8B8A8_SRGB,
    .extent = msColorTex->Extent(),
    .mipLevels = 1,
    .arrayLayers = 1,
    .sampleCount = Fwog::SampleCount::SAMPLES_1,
  });
}

void MultisampleApplication::OnRender(double dt)
{
  timeAccum += dt * 0.02;
  timeBuffer.UpdateData(timeAccum);

  auto attachment = Fwog::RenderColorAttachment{
    .texture = msColorTex.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.2f, .0f, .2f, 1.0f},
  };

  Fwog::Render(
    {
      .colorAttachments = {&attachment, 1},
    },
    [&]
    {
      Fwog::Cmd::BindGraphicsPipeline(pipeline);
      Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
      Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));
      Fwog::Cmd::BindUniformBuffer(0, timeBuffer);
      Fwog::Cmd::Draw(3, 1, 0, 0);
    });

  // Resolve multisample texture by blitting it to a same-size non-multisample texture
  Fwog::BlitTexture(*msColorTex, *resolveColorTex, {}, {}, msColorTex->Extent(), resolveColorTex->Extent(), Fwog::Filter::LINEAR);

  // Blit resolved texture to screen with nearest neighbor filter to make MSAA resolve more obvious
  Fwog::BlitTextureToSwapchain(*resolveColorTex,
                               {},
                               {},
                               resolveColorTex->Extent(),
                               {windowWidth, windowHeight, 1},
                               Fwog::Filter::NEAREST);
}

void MultisampleApplication::OnGui(double dt)
{
  ImGui::Begin("Options");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Text("Max samples: %d", Fwog::GetDeviceProperties().limits.maxSamples);
  ImGui::RadioButton("1 Sample", (int*)&numSamples, 1);
  ImGui::RadioButton("2 Samples", (int*)&numSamples, 2);
  ImGui::RadioButton("4 Samples", (int*)&numSamples, 4);
  ImGui::RadioButton("8 Samples", (int*)&numSamples, 8);
  ImGui::RadioButton("16 Samples", (int*)&numSamples, 16);
  ImGui::RadioButton("32 Samples", (int*)&numSamples, 32);
  ImGui::End();

  if (numSamples != msColorTex->GetCreateInfo().sampleCount)
  {
    OnWindowResize(windowWidth, windowHeight);
  }
}

int main()
{
  auto appInfo = Application::CreateInfo{
    .name = "MSAA",
    .maximize = false,
    .decorate = true,
    .vsync = true,
  };
  auto app = MultisampleApplication(appInfo);

  app.Run();

  return 0;
}
