#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <exception>
#include <format>
#include <iostream>
#include <sstream>
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

static void GLAPIENTRY glErrorCallback(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  [[maybe_unused]] GLsizei length,
  const GLchar* message,
  [[maybe_unused]] const void* userParam)
{
  // ignore insignificant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204 || id == 0
    )//|| id == 131188 || id == 131186)
    return;

  std::stringstream errStream;
  errStream << "OpenGL Debug message (" << id << "): " << message << '\n';

  switch (source)
  {
  case GL_DEBUG_SOURCE_API:             errStream << "Source: API"; break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   errStream << "Source: Window Manager"; break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER: errStream << "Source: Shader Compiler"; break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:     errStream << "Source: Third Party"; break;
  case GL_DEBUG_SOURCE_APPLICATION:     errStream << "Source: Application"; break;
  case GL_DEBUG_SOURCE_OTHER:           errStream << "Source: Other"; break;
  }

  errStream << '\n';

  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR:               errStream << "Type: Error"; break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: errStream << "Type: Deprecated Behaviour"; break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  errStream << "Type: Undefined Behaviour"; break;
  case GL_DEBUG_TYPE_PORTABILITY:         errStream << "Type: Portability"; break;
  case GL_DEBUG_TYPE_PERFORMANCE:         errStream << "Type: Performance"; break;
  case GL_DEBUG_TYPE_MARKER:              errStream << "Type: Marker"; break;
  case GL_DEBUG_TYPE_PUSH_GROUP:          errStream << "Type: Push Group"; break;
  case GL_DEBUG_TYPE_POP_GROUP:           errStream << "Type: Pop Group"; break;
  case GL_DEBUG_TYPE_OTHER:               errStream << "Type: Other"; break;
  }

  errStream << '\n';

  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH:
    errStream << "Severity: high";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    errStream << "Severity: medium";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    errStream << "Severity: low";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    errStream << "Severity: notification";
    break;
  }

  std::cout << errStream.str() << '\n';
}

static GLuint CompileShader(GLenum stage, std::string_view source)
{
  auto sourceStr = std::string(source);
  const GLchar* strings = sourceStr.c_str();

  GLuint shader = glCreateShader(stage);
  glShaderSource(shader, 1, &strings, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    GLsizei infoLength = 512;
    std::string infoLog(infoLength + 1, '\0');
    glGetShaderInfoLog(shader, infoLength, nullptr, infoLog.data());

    throw std::runtime_error(infoLog);
  }

  return shader;
}

static void LinkProgram(GLuint program)
{
  glLinkProgram(program);
  GLsizei length = 512;

  GLint success{};
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success)
  {
    std::string infoLog(length + 1, '\0');
    glGetProgramInfoLog(program, length, nullptr, infoLog.data());

    throw std::runtime_error(infoLog);
  }
}

static GLuint GenerateProgram(std::string_view vs, std::string_view fs)
{
  GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vs);
  GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fs);

  GLuint program = glCreateProgram();

  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  try { LinkProgram(program); }
  catch (std::runtime_error& e) { glDeleteProgram(program); throw e; }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  return program;
}

struct WindowCreateInfo
{
  bool maximize{};
  bool decorate{};
  uint32_t width{};
  uint32_t height{};
};

GLFWwindow* CreateWindow(const WindowCreateInfo& createInfo)
{
  if (!glfwInit())
  {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  glfwSetErrorCallback([](int, const char* desc)
    {
      std::cout << std::format("GLFW error: {}\n", desc);
    });

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, createInfo.maximize);
  glfwWindowHint(GLFW_DECORATED, createInfo.decorate);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

  const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  GLFWwindow* window = glfwCreateWindow(createInfo.width, createInfo.height, "Example Giraffics", nullptr, nullptr);

  if (!window)
  {
    throw std::runtime_error("Failed to create window");
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  return window;
}

void InitOpenGL()
{
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0)
  {
    throw std::runtime_error("Failed to initialize OpenGL");
  }
}

int main()
{
  GLFWwindow* window = CreateWindow({ .maximize = false, .decorate = true, .width = 1280, .height = 720 });
  InitOpenGL();

  glEnable(GL_FRAMEBUFFER_SRGB);

  // enable debugging stuff
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(glErrorCallback, NULL);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

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

  GLuint shader = GenerateProgram(gVertexSource, gFragmentSource);
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
    GFX::Cmd::BindPipeline(pipeline);
    GFX::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, 2 * sizeof(float));
    GFX::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, 3 * sizeof(uint8_t));
    GFX::Cmd::Draw(3, 1, 0, 0);
    GFX::EndRendering();

    glfwSwapBuffers(window);
  }

  glfwTerminate();

  return 0;
}