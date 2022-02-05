#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string_view>

namespace Utility
{
  void GLAPIENTRY glErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

  GLuint CompileShader(GLenum stage, std::string_view source);
  void LinkProgram(GLuint program);
  GLuint CompileVertexFragmentProgram(std::string_view vs, std::string_view fs);

  struct WindowCreateInfo
  {
    bool maximize{};
    bool decorate{};
    uint32_t width{};
    uint32_t height{};
  };

  GLFWwindow* CreateWindow(const WindowCreateInfo& createInfo);
  void InitOpenGL();
}