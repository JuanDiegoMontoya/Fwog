#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string_view>
#include <string>

namespace Utility
{
  void GLAPIENTRY glErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

  struct WindowCreateInfo
  {
    std::string_view name = "";
    bool maximize{};
    bool decorate{};
    uint32_t width{};
    uint32_t height{};
  };

  GLFWwindow* CreateWindow(const WindowCreateInfo& createInfo);
  void InitOpenGL();

  std::string LoadFile(std::string_view path);
}
