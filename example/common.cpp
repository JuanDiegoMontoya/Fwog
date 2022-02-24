#include "common.h"
#include <exception>
#include <iostream>
#include <sstream>
#include <fstream>

namespace Utility
{
  void GLAPIENTRY glErrorCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    [[maybe_unused]] GLsizei length,
    const GLchar* message,
    [[maybe_unused]] const void* userParam)
  {
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204 || id == 0)
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

  GLuint CompileShader(GLenum stage, std::string_view source)
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

  void LinkProgram(GLuint program)
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

  GLuint CompileVertexFragmentProgram(std::string_view vs, std::string_view fs)
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

  GLuint CompileComputeProgram(std::string_view cs)
  {
    GLuint vertexShader = CompileShader(GL_COMPUTE_SHADER, cs);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);

    try { LinkProgram(program); }
    catch (std::runtime_error& e) { glDeleteProgram(program); throw e; }

    glDeleteShader(vertexShader);
    return program;
  }

  GLFWwindow* CreateWindow(const WindowCreateInfo& createInfo)
  {
    if (!glfwInit())
    {
      throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwSetErrorCallback([](int, const char* desc)
      {
        std::cout << "GLFW error: " << desc << '\n';
      });

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, createInfo.maximize);
    glfwWindowHint(GLFW_DECORATED, createInfo.decorate);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(createInfo.width, createInfo.height, createInfo.name.data(), nullptr, nullptr);

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

    // enable debugging stuff
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(Utility::glErrorCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  }

  std::string LoadFile(std::string_view path)
  {
    std::ifstream file{ path.data() };
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
  }
}