#include "Application.h"
#include "AppImage.h"

#include <Fwog/Context.h>
#include <Fwog/DebugMarker.h>

#include FWOG_OPENGL_HEADER
#include <GLFW/glfw3.h>

#include "../vendor/stb_image.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/constants.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

// Use the high-performance GPU (if available) on Windows laptops
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
#ifdef _WIN32
extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace
{
  void GLAPIENTRY OpenglErrorCallback(GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      [[maybe_unused]] GLsizei length,
                                      const GLchar* message,
                                      [[maybe_unused]] const void* userParam)
  {
    // Ignore certain verbose info messages (particularly ones on Nvidia).
    if (id == 131169 || 
        id == 131185 || // NV: Buffer will use video memory
        id == 131218 || 
        id == 131204 || // Texture cannot be used for texture mapping
        id == 131222 ||
        id == 131154 || // NV: pixel transfer is synchronized with 3D rendering
        id == 0         // gl{Push, Pop}DebugGroup
      )
      return;

    std::stringstream errStream;
    errStream << "OpenGL Debug message (" << id << "): " << message << '\n';

    switch (source)
    {
    case GL_DEBUG_SOURCE_API: errStream << "Source: API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: errStream << "Source: Window Manager"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: errStream << "Source: Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY: errStream << "Source: Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION: errStream << "Source: Application"; break;
    case GL_DEBUG_SOURCE_OTHER: errStream << "Source: Other"; break;
    }

    errStream << '\n';

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR: errStream << "Type: Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: errStream << "Type: Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: errStream << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY: errStream << "Type: Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE: errStream << "Type: Performance"; break;
    case GL_DEBUG_TYPE_MARKER: errStream << "Type: Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP: errStream << "Type: Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP: errStream << "Type: Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER: errStream << "Type: Other"; break;
    }

    errStream << '\n';

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH: errStream << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM: errStream << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW: errStream << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: errStream << "Severity: notification"; break;
    }

    std::cout << errStream.str() << '\n';
  }
} // namespace

// This class provides static callbacks for GLFW.
// It has access to the private members of Application and assumes a pointer to it is present in the window's user pointer.
class ApplicationAccess
{
public:
  static void CursorPosCallback(GLFWwindow* window, double currentCursorX, double currentCursorY)
  {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app->cursorJustEnteredWindow)
    {
      app->previousCursorPos = {currentCursorX, currentCursorY};
      app->cursorJustEnteredWindow = false;
    }

    app->cursorFrameOffset +=
        glm::dvec2{currentCursorX - app->previousCursorPos.x, app->previousCursorPos.y - currentCursorY};
    app->previousCursorPos = {currentCursorX, currentCursorY};
  }

  static void CursorEnterCallback(GLFWwindow* window, int entered)
  {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (entered)
    {
      app->cursorJustEnteredWindow = true;
    }
  }

  static void WindowResizeCallback(GLFWwindow* window, int, int)
  {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    int newWidth{};
    int newHeight{};
    glfwGetFramebufferSize(window, &newWidth, &newHeight);
    app->windowWidth = static_cast<uint32_t>(newWidth);
    app->windowHeight = static_cast<uint32_t>(newHeight);

    if (newWidth > 0 && newHeight > 0)
    {
      app->OnWindowResize(app->windowWidth, app->windowHeight);
      app->Draw(0);
    }
  }
};

std::string Application::LoadFile(const std::filesystem::path& path)
{
  std::ifstream file{path};
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::pair<std::unique_ptr<std::byte[]>, std::size_t> Application::LoadBinaryFile(const std::filesystem::path& path)
{
  std::size_t fsize = std::filesystem::file_size(path);
  auto memory = std::make_unique<std::byte[]>(fsize);
  std::ifstream file{path, std::ifstream::binary};
  std::copy(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), reinterpret_cast<char*>(memory.get()));
  return {std::move(memory), fsize};
}

Application::Application(const CreateInfo& createInfo)
{
  // Initialiize GLFW
  if (!glfwInit())
  {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  glfwSetErrorCallback([](int, const char* desc) { std::cout << "GLFW error: " << desc << '\n'; });

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, createInfo.maximize);
  glfwWindowHint(GLFW_DECORATED, createInfo.decorate);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  if (monitor == nullptr)
  {
    throw std::runtime_error("No Monitor detected");
  }
  const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
  window = glfwCreateWindow(static_cast<int>(videoMode->width * .75), static_cast<int>(videoMode->height * .75), createInfo.name.data(), nullptr, nullptr);
  if (!window)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to create window");
  }

  int appImageWidth{};
  int appImageHeight{};
  unsigned char* appImagePixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(&gAppImage[0]), sizeof(gAppImage), &appImageWidth, &appImageHeight, nullptr, 4);
  if (appImagePixels != nullptr)
  {
    GLFWimage appImage;
    appImage.width = 128;
    appImage.height = 128;
    appImage.pixels = appImagePixels;
    glfwSetWindowIcon(window, 1, &appImage);
    stbi_image_free(appImagePixels);
  }

  int xSize{};
  int ySize{};
  glfwGetFramebufferSize(window, &xSize, &ySize);
  windowWidth = static_cast<uint32_t>(xSize);
  windowHeight = static_cast<uint32_t>(ySize);

  int monitorLeft{};
  int monitorTop{};
  glfwGetMonitorPos(monitor, &monitorLeft, &monitorTop);

  glfwSetWindowPos(window, videoMode->width / 2 - windowWidth / 2 + monitorLeft, videoMode->height / 2 - windowHeight / 2 + monitorTop);

  glfwSetWindowUserPointer(window, this);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(createInfo.vsync ? 1 : 0);

  glfwSetCursorPosCallback(window, ApplicationAccess::CursorPosCallback);
  glfwSetCursorEnterCallback(window, ApplicationAccess::CursorEnterCallback);
  glfwSetFramebufferSizeCallback(window, ApplicationAccess::WindowResizeCallback);

  // Initialize OpenGL.
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to initialize OpenGL");
  }

  // Set up the GL debug message callback.
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(OpenglErrorCallback, nullptr);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

  //auto fwogCallback = [](std::string_view msg) { printf("Fwog: %.*s\n", static_cast<int>(msg.size()), msg.data()); };
  auto fwogCallback = nullptr;
  Fwog::Initialize({.verboseMessageCallback = fwogCallback});

  // Initialize ImGui and a backend for it.
  // Because we allow the GLFW backend to install callbacks, it will automatically call our own that we provided.
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsDark();
}

Application::~Application()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  Fwog::Terminate();
  glfwTerminate();
}

void Application::Draw(double dt)
{
  glEnable(GL_FRAMEBUFFER_SRGB);

  // Start a new ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (windowWidth > 0 && windowHeight > 0)
  {
    OnRender(dt);
    OnGui(dt);
  }

  // Updates ImGui.
  // A frame marker is inserted to distinguish ImGui rendering from the application's in a debugger.
  {
    ImGui::Render();
    auto* drawData = ImGui::GetDrawData();
    if (drawData->CmdListsCount > 0)
    {
      auto marker = Fwog::ScopedDebugMarker("Draw GUI");
      glDisable(GL_FRAMEBUFFER_SRGB);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
  }

  glfwSwapBuffers(window);
}

void Application::Run()
{
  glfwSetInputMode(window, GLFW_CURSOR, cursorIsActive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

  // The main loop.
  double prevFrame = glfwGetTime();
  while (!glfwWindowShouldClose(window))
  {
    double curFrame = glfwGetTime();
    double dt = curFrame - prevFrame;
    prevFrame = curFrame;

    cursorFrameOffset = {0.0, 0.0};
    glfwPollEvents();

    // Close the app if the user presses Escape.
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    // Toggle the cursor if the grave accent (tilde) key is pressed.
    if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) && graveHeldLastFrame == false)
    {
      cursorIsActive = !cursorIsActive;
      cursorJustEnteredWindow = true;
      graveHeldLastFrame = true;
      glfwSetInputMode(window, GLFW_CURSOR, cursorIsActive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    if (!glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT))
    {
      graveHeldLastFrame = false;
    }
    
    // Prevent the cursor from clicking ImGui widgets when it is disabled.
    if (!cursorIsActive)
    {
      glfwSetCursorPos(window, 0, 0);
      previousCursorPos.x = 0;
      previousCursorPos.y = 0;
    }

    // Update the main mainCamera.
    // WASD can be used to move the camera forwards, backwards, and side-to-side.
    // The mouse can be used to orient the camera.
    // Not all examples will use the main camera.
    if (!cursorIsActive)
    {
      const float dtf = static_cast<float>(dt);
      const glm::vec3 forward = mainCamera.GetForwardDir();
      const glm::vec3 right = glm::normalize(glm::cross(forward, {0, 1, 0}));
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        mainCamera.position += forward * dtf * cameraSpeed;
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        mainCamera.position -= forward * dtf * cameraSpeed;
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        mainCamera.position += right * dtf * cameraSpeed;
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        mainCamera.position -= right * dtf * cameraSpeed;
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        mainCamera.position.y -= dtf * cameraSpeed;
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        mainCamera.position.y += dtf * cameraSpeed;
      mainCamera.yaw += static_cast<float>(cursorFrameOffset.x * cursorSensitivity);
      mainCamera.pitch += static_cast<float>(cursorFrameOffset.y * cursorSensitivity);
      mainCamera.pitch = glm::clamp(mainCamera.pitch, -glm::half_pi<float>() + 1e-4f, glm::half_pi<float>() - 1e-4f);
    }

    // Call the application's overriden functions each frame.
    OnUpdate(dt);

    Draw(dt);
  }
}