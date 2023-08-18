#include "common/Application.h"
#include "common/SceneLoader.h"

#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>
#include <Fwog/Texture.h>
#include <Fwog/Timer.h>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>

#include <array>
#include <charconv>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

/* 05_gpu_driven
 *
 * A basic GPU-driven renderer. Occlusion culling is performed by rendering object bounding boxes with early fragment
 * tests enabled. If any fragments are drawn, then the object is potentially visible and is marked to be rendered.
 * Then, the entire scene is drawn in a single draw call using DrawIndexedIndirect and bindless textures (taking
 * care not to invoke undefined behavior).
 *
 * The app has the same options as 03_gltf_viewer.
 *
 * Options
 * Filename (string) : name of the glTF file you wish to view.
 * Scale (real)      : uniform scale factor in case the model is tiny or huge. Default: 1.0
 * Binary (int)      : whether the input file is binary glTF. Default: false
 *
 * If no options are specified, the default scene will be loaded.
 *
 * * Shown (+ indicates new features):
 * - Creating vertex buffers
 * - Specifying vertex attributes
 * - Loading shaders
 * - Creating a graphics pipeline
 * - Rendering to the screen
 * - Dynamic uniform buffers
 * - Memory barriers
 * + Indirect drawing
 * + Bindless textures
 *
 * TODO: frustum culling
 * TODO: hi-z occlusion culling
 * TODO: disocclusion fixup pass
 */

struct alignas(16) ObjectUniforms
{
  glm::mat4 model;
  uint32_t materialIdx;
};

struct alignas(16) BoundingBox
{
  glm::vec3 offset;
  uint32_t padding_0;
  glm::vec3 halfExtent;
  uint32_t padding_1;
};

struct GlobalUniforms
{
  glm::mat4 viewProj;
  glm::mat4 invViewProj;
  glm::vec4 cameraPos;
};

struct ShadingUniforms
{
  glm::mat4 sunViewProj;
  glm::vec4 sunDir;
  glm::vec4 sunStrength;
};

std::string LoadFileWithInclude(std::string_view path, std::string_view includeDir)
{
  char error[256] = {};
  char* included = stb_include_string(Application::LoadFile(path).data(), nullptr, includeDir.data(), "", error);
  std::string includedStr = included;
  free(included);
  return includedStr;
}

constexpr std::array<Fwog::VertexInputBindingDescription, 3> sceneInputBindingDescs = {
  Fwog::VertexInputBindingDescription{
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32B32_FLOAT,
    .offset = offsetof(Utility::Vertex, position),
  },
  Fwog::VertexInputBindingDescription{
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R16G16_SNORM,
    .offset = offsetof(Utility::Vertex, normal),
  },
  Fwog::VertexInputBindingDescription{
    .location = 2,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = offsetof(Utility::Vertex, texcoord),
  },
};

Fwog::GraphicsPipeline CreateScenePipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/SceneForward.vert.glsl", "shaders/gpu_driven"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/SceneForward.frag.glsl", "shaders/gpu_driven"));

  return Fwog::GraphicsPipeline({
    .name = "Generic material",
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .vertexInputState = {sceneInputBindingDescs},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = Fwog::CompareOp::LESS},
  });
}

Fwog::GraphicsPipeline CreateBoundingBoxDebugPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/BoundingBox.vert.glsl", "shaders/gpu_driven"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/SolidColor.frag.glsl", "shaders/gpu_driven"));

  return Fwog::GraphicsPipeline({
    .name = "Wireframe bounding boxes",
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_STRIP},
    .rasterizationState = {.polygonMode = Fwog::PolygonMode::LINE, .cullMode = Fwog::CullMode::NONE},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = Fwog::CompareOp::LESS},
  });
}

Fwog::GraphicsPipeline CreateBoundingBoxCullingPipeline()
{
  auto vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/BoundingBox.vert.glsl", "shaders/gpu_driven"));
  auto fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER,
                         LoadFileWithInclude("shaders/gpu_driven/CullVisibility.frag.glsl", "shaders/gpu_driven"));

  return Fwog::GraphicsPipeline({
    .name = "Culling bounding boxes",
    .vertexShader = &vs,
    .fragmentShader = &fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_STRIP},
    .rasterizationState = {.polygonMode = Fwog::PolygonMode::FILL, .cullMode = Fwog::CullMode::NONE},
    .depthState = {.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = Fwog::CompareOp::LESS},
  });
}

class GpuDrivenApplication final : public Application
{
public:
  GpuDrivenApplication(const Application::CreateInfo& createInfo,
                       std::optional<std::string_view> filename,
                       float scale,
                       bool binary);

private:
  void OnWindowResize(uint32_t newWidth, uint32_t newHeight) override;
  void OnUpdate(double dt) override;
  void OnRender(double dt) override;
  void OnGui(double dt) override;

  // Config info.
  struct
  {
    float viewNearPlane = 0.3f;
    bool freezeCulling = false;
    bool viewBoundingBoxes = false;
  } config;

  // Resources tied to the swapchain/output size
  struct Frame
  {
    // g-buffer textures
    std::optional<Fwog::Texture> gAlbedo;
    std::optional<Fwog::Texture> gDepth;
  };
  Frame frame{};

  Fwog::GraphicsPipeline scenePipeline;
  Fwog::GraphicsPipeline boundingBoxDebugPipeline;
  Fwog::GraphicsPipeline boundingBoxCullingPipeline;

  Fwog::TypedBuffer<GlobalUniforms> globalUniformsBuffer;

  // Scene
  Utility::SceneBindless scene;
  std::vector<Fwog::DrawIndexedIndirectCommand> drawCommands;
  std::optional<Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>> drawCommandsBuffer;
  std::optional<Fwog::TypedBuffer<Utility::Vertex>> vertexBuffer;
  std::optional<Fwog::TypedBuffer<Utility::index_t>> indexBuffer;
  std::optional<Fwog::TypedBuffer<ObjectUniforms>> meshUniformBuffer;
  std::optional<Fwog::TypedBuffer<BoundingBox>> boundingBoxesBuffer;
  std::optional<Fwog::Buffer> objectIndicesBuffer; // Unused
  std::optional<Fwog::TypedBuffer<Utility::GpuMaterialBindless>> materialsBuffer;
};

GpuDrivenApplication::GpuDrivenApplication(const Application::CreateInfo& createInfo,
                                           std::optional<std::string_view> filename,
                                           float scale,
                                           bool binary)
  : Application(createInfo),
    scenePipeline(CreateScenePipeline()),
    boundingBoxDebugPipeline(CreateBoundingBoxDebugPipeline()),
    boundingBoxCullingPipeline(CreateBoundingBoxCullingPipeline()),
    globalUniformsBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE)
{
  bool success = false;

  if (!filename)
  {
    success = Utility::LoadModelFromFileBindless(scene, "models/simple_scene.glb", glm::mat4{.5}, true);
  }
  else
  {
    success = Utility::LoadModelFromFileBindless(scene, *filename, glm::scale(glm::vec3{scale}), binary);
  }

  if (!success)
  {
    throw std::runtime_error("Failed to load scene");
  }

  std::vector<ObjectUniforms> meshUniforms;
  std::vector<BoundingBox> boundingBoxes;
  std::vector<uint32_t> objectIndices = {static_cast<uint32_t>(scene.meshes.size())};

  int curObjectIndex = 0;
  for (const auto& mesh : scene.meshes)
  {
    // The mesh uniforms are indexed with gl_DrawID (each mesh gets one set of uniforms).
    meshUniforms.push_back(ObjectUniforms{.model = mesh.transform, .materialIdx = mesh.materialIdx});
    // Each mesh has a bounding box which is used as its cheap-to-draw proxy volume for occlusion culling.
    boundingBoxes.push_back(BoundingBox{
      .offset = mesh.boundingBox.offset,
      .halfExtent = mesh.boundingBox.halfExtent,
    });
    // Initialize the indirect draw command. Note that the instance count is initialized to 0.
    // The other draw parameters depend on the mesh's location in the one big vertex buffer.
    drawCommands.push_back(Fwog::DrawIndexedIndirectCommand{
      .indexCount = mesh.indexCount,
      .instanceCount = 0,
      .firstIndex = mesh.startIndex,
      .vertexOffset = mesh.startVertex,
      .firstInstance = 0,
    });
    // Currently unused.
    objectIndices.push_back(curObjectIndex++);
  }

  drawCommandsBuffer = Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>(drawCommands);
  vertexBuffer = Fwog::TypedBuffer<Utility::Vertex>(scene.vertices);
  indexBuffer = Fwog::TypedBuffer<Utility::index_t>(scene.indices);
  meshUniformBuffer = Fwog::TypedBuffer<ObjectUniforms>(meshUniforms);
  boundingBoxesBuffer = Fwog::TypedBuffer<BoundingBox>(boundingBoxes);
  objectIndicesBuffer = Fwog::Buffer(std::span(objectIndices));
  materialsBuffer = Fwog::TypedBuffer<Utility::GpuMaterialBindless>(scene.materials);

  mainCamera.position = {0, 1.5, 2};
  mainCamera.yaw = -glm::half_pi<float>();

  OnWindowResize(windowWidth, windowHeight);
}

void GpuDrivenApplication::OnWindowResize(uint32_t newWidth, uint32_t newHeight)
{
  frame.gAlbedo = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::R8G8B8A8_SRGB);
  frame.gDepth = Fwog::CreateTexture2D({newWidth, newHeight}, Fwog::Format::D32_FLOAT);
}

void GpuDrivenApplication::OnUpdate([[maybe_unused]] double dt) {}

void GpuDrivenApplication::OnRender([[maybe_unused]] double dt)
{
  const auto fovy = glm::radians(60.f);
  const auto aspectRatio = windowWidth / (float)windowHeight;
  auto proj = glm::perspective(fovy, aspectRatio, 0.3f, 100.f);

  // Update global per-frame uniforms
  GlobalUniforms mainCameraUniforms{};
  mainCameraUniforms.viewProj = proj * mainCamera.GetViewMatrix();
  mainCameraUniforms.invViewProj = glm::inverse(mainCameraUniforms.viewProj);
  mainCameraUniforms.cameraPos = glm::vec4(mainCamera.position, 0.0);
  globalUniformsBuffer.UpdateData(mainCameraUniforms);

  auto gDepthAttachment = Fwog::RenderDepthStencilAttachment{
    .texture = frame.gDepth.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.depth = 1.0f},
  };

  // Scene pass. Draw everything that was marked visible in the previous frame's culling pass.
  auto gColorAttachment = Fwog::RenderColorAttachment{
    .texture = frame.gAlbedo.value(),
    .loadOp = Fwog::AttachmentLoadOp::CLEAR,
    .clearValue = {.1f, .3f, .5f, 0.0f},
  };
  Fwog::Render(
    {
      .name = "Scene",
      .colorAttachments = std::span(&gColorAttachment, 1),
      .depthAttachment = gDepthAttachment,
    },
    [&]
    {
      Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::COMMAND_BUFFER_BIT | Fwog::MemoryBarrierBit::SHADER_STORAGE_BIT);

      Fwog::Cmd::BindGraphicsPipeline(scenePipeline);

      Fwog::Cmd::BindUniformBuffer("GlobalUniforms", globalUniformsBuffer);
      Fwog::Cmd::BindStorageBuffer("ObjectUniformsBuffer", meshUniformBuffer.value());
      Fwog::Cmd::BindStorageBuffer("MaterialUniforms", materialsBuffer.value());
      Fwog::Cmd::BindStorageBuffer("BoundingBoxesBuffer", boundingBoxesBuffer.value());
      Fwog::Cmd::BindStorageBuffer("ObjectIndicesBuffer", objectIndicesBuffer.value());

      Fwog::Cmd::BindVertexBuffer(0, vertexBuffer.value(), 0, sizeof(Utility::Vertex));
      Fwog::Cmd::BindIndexBuffer(indexBuffer.value(), Fwog::IndexType::UNSIGNED_INT);
      Fwog::Cmd::DrawIndexedIndirect(drawCommandsBuffer.value(), 0, static_cast<uint32_t>(scene.meshes.size()), 0);

      if (config.viewBoundingBoxes)
      {
        Fwog::Cmd::BindGraphicsPipeline(boundingBoxDebugPipeline);
        Fwog::Cmd::Draw(14, static_cast<uint32_t>(scene.meshes.size()), 0, 0);
      }
    });

  // Draw culling boxes. If any fragment is visible, objects have their instance count set to 1.
  // This pass comes after the scene pass because it relies on a depth buffer to have already been created.
  // That means objects will become visible exactly 1 frame after being disoccluded. This is generally not
  // noticeable unless at low framerates.
  if (!config.freezeCulling)
  {
    gDepthAttachment.loadOp = Fwog::AttachmentLoadOp::LOAD;
    Fwog::Render(
      {
        .name = "Occlusion culling",
        .depthAttachment = gDepthAttachment,
      },
      [&]
      {
        // Re-upload the draw commands buffer to reset the instance counts to 0 for culling.
        // Ideally, this would be a compute pass where the draw commands are completely regenerated (e.g.,
        // with frustum culling), or the instance counts are reset to 0.
        drawCommandsBuffer = Fwog::TypedBuffer<Fwog::DrawIndexedIndirectCommand>(drawCommands);

        // Draw visible bounding boxes.
        Fwog::Cmd::BindGraphicsPipeline(boundingBoxCullingPipeline);

        Fwog::Cmd::BindUniformBuffer("GlobalUniforms", globalUniformsBuffer);
        Fwog::Cmd::BindStorageBuffer("ObjectUniformsBuffer", meshUniformBuffer.value());
        Fwog::Cmd::BindStorageBuffer("MaterialUniforms", materialsBuffer.value());
        Fwog::Cmd::BindStorageBuffer("BoundingBoxesBuffer", boundingBoxesBuffer.value());
        Fwog::Cmd::BindStorageBuffer("ObjectIndicesBuffer", objectIndicesBuffer.value());
        Fwog::Cmd::BindStorageBuffer("DrawCommandsBuffer", drawCommandsBuffer.value());

        // TODO: upgrade to indirect draw after frustum culling is added.
        Fwog::Cmd::Draw(14, static_cast<uint32_t>(scene.meshes.size()), 0, 0);
      });
  }

  Fwog::BlitTextureToSwapchain(frame.gAlbedo.value(),
                               {},
                               {},
                               {windowWidth, windowHeight},
                               {windowWidth, windowHeight},
                               Fwog::Filter::NEAREST);
}

void GpuDrivenApplication::OnGui(double dt)
{
  ImGui::Begin("Options");
  ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
  ImGui::Checkbox("Freeze culling", &config.freezeCulling);
  ImGui::Checkbox("View bounding boxes", &config.viewBoundingBoxes);
  ImGui::End();
}

int main(int argc, const char* const* argv)
{
  std::optional<std::string_view> filename;
  float scale = 1.0f;
  bool binary = false;

  try
  {
    if (argc > 1)
    {
      filename = argv[1];
    }
    if (argc > 2)
    {
      scale = std::stof(argv[2]);
    }
    if (argc > 3)
    {
      int val = 0;
      auto [ptr, ec] = std::from_chars(argv[3], argv[3] + strlen(argv[3]), val);
      binary = static_cast<bool>(val);
      if (ec != std::errc{})
      {
        throw std::runtime_error("Binary should be 0 or 1");
      }
    }
  }
  catch (std::exception& e)
  {
    printf("Argument parsing error: %s\n", e.what());
    return -1;
  }

  auto appInfo = Application::CreateInfo{.name = "GPU-Driven Renderer Example"};
  auto app = GpuDrivenApplication(appInfo, filename, scale, binary);
  app.Run();

  return 0;
}