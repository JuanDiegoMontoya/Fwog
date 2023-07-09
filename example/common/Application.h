#pragma once
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>
#include <string>
#include <utility>

#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct GLFWwindow;

// Represents the camera's position and orientation.
struct View
{
  glm::vec3 position{};
  float pitch{}; // pitch angle in radians
  float yaw{};   // yaw angle in radians

  glm::vec3 GetForwardDir() const
  {
    return glm::vec3{cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw)};
  }

  glm::mat4 GetViewMatrix() const
  {
    return glm::lookAt(position, position + GetForwardDir(), glm::vec3(0, 1, 0));
  }
};

class Application
{
public:
  struct CreateInfo
  {
    std::string_view name = "";
    bool maximize = false;
    bool decorate = true;
    bool vsync = true;
  };

  // TODO: An easy way to load shaders should probably be a part of Fwog
  static std::string LoadFile(const std::filesystem::path& path);
  static std::pair<std::unique_ptr<std::byte[]>, std::size_t> LoadBinaryFile(const std::filesystem::path& path);

  Application(const CreateInfo& createInfo);
  Application(const Application&) = delete;
  Application(Application&&) noexcept = delete;
  Application& operator=(const Application&) = delete;
  Application& operator=(Application&&) noexcept = delete;

  virtual ~Application();

  void Run();

protected:
  virtual void OnWindowResize([[maybe_unused]] uint32_t newWidth, [[maybe_unused]] uint32_t newHeight){};
  virtual void OnUpdate([[maybe_unused]] double dt){};
  virtual void OnRender([[maybe_unused]] double dt){};
  virtual void OnGui([[maybe_unused]] double dt){};

  GLFWwindow* window;
  View mainCamera{};
  float cursorSensitivity = 0.0025f;
  float cameraSpeed = 4.5f;
  bool cursorIsActive = true;
  
  uint32_t windowWidth{};
  uint32_t windowHeight{};

private:
  friend class ApplicationAccess;

  void Draw(double dt);

  glm::dvec2 previousCursorPos{};
  glm::dvec2 cursorFrameOffset{};
  bool cursorJustEnteredWindow = true;
  bool graveHeldLastFrame = false;
};