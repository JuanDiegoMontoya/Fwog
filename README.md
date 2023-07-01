# Fwog

![Fwog logo](media/logo.png)

[![Docs](https://img.shields.io/readthedocs/fwog?style=for-the-badge)](https://fwog.readthedocs.io) [![CI](https://img.shields.io/github/actions/workflow/status/juandiegomontoya/fwog/cmake.yml?branch=main&style=for-the-badge)](https://github.com/juandiegomontoya/fwog/actions/workflows/cmake.yml)

## Froggy OpenGL Engoodener

Low-level OpenGL 4.6 abstraction written in C++20. Fwog mitigates many of the weak points of OpenGL while providing a modern interface (outlined in [my blog post](https://juandiegomontoya.github.io/modern_opengl.html)).

## Core Features

- Type-safe RAII wrappers for most API objects (buffers, textures, samplers, fences, and more)
- Type-safe enums and bit flags
- Simplified render target specification
- Encapsulate global pipeline state into pipeline objects
- Prevent state leaking bugs by using explicit rendering scopes

## Divergence from OpenGL

Fwog is interested in providing an abstraction of a *subset* of OpenGL. That means several features are not exposed. Here is a non-exhaustive list of them:

- The default uniform block (e.g., uniforms set with glUniform*)
  - Alternative: uniform and storage buffers
- Geometry shaders
  - Alternative: compute shaders
- Multiple viewports/layered rendering
- Transform feedback
  - Alternative: storage buffers
- Hardware occlusion queries
  - Alternative: storage buffer + fence + readback
- SPIR-V shaders
- ...and probably many more features are not exposed

If an issue is raised about a missing feature, I might add it. If a PR is made that implements it, I will probably merge it.

## Future Features

Fwog is essentially feature-complete, but there are some smaller things that need attention. See [the issues](https://github.com/JuanDiegoMontoya/Fwog/issues) for features that are being considered or worked on.

Bear in mind that the API is regularly broken as improvements are made. Consult the releases page for stable-r releases.

## Getting Started

### Installing

```bash
$ git clone https://github.com/JuanDiegoMontoya/Fwog.git
$ cd Fwog
$ mkdir build
$ cd build
$ cmake ..
```

### Programming

After you have created a window and OpenGL context (e.g., with GLFW) and loaded OpenGL function pointers (e.g., with Glad), you can call `Fwog::Initialize()`. This initializes some internal structures used by Fwog for tracking state. Remember to eventually call `Fwog::Terminate()` before the program closes and while the context is still active. These functions are declared in `<Fwog/Context.h>`.

For this example, we will need some additional includes.

```cpp
#include <Fwog/Shader.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Rendering.h>
```

Instead of having a bunch of hidden global state, Fwog, like Vulkan and D3D12, uses the notion of pipelines to encapsulate it in a single object. To create a pipeline, we first need a shader.

```cpp
auto vertexShader = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, vertexShaderSource);
```

`vertexShaderSource` is a GLSL shader source string, as most OpenGL users are familiar with.

A vertex shader alone is sufficient to create a graphics pipeline.

```cpp
auto pipeline = Fwog::GraphicsPipeline{{.vertexShader = &vertexShader}};
```

This will create a pipeline with a vertex shader and a bunch of (sensible) default state. See [Pipeline.h](https://github.com/JuanDiegoMontoya/Fwog/blob/main/include/Fwog/Pipeline.h) to see what this entails.

To use the pipeline, we need to be in a rendering scope. To do this, we need to call `Render` (to render to off-screen render targets) or `RenderToSwapchain` (to render directly to the screen). Both functions take a callable that invokes rendering commands.

```cpp
Fwog::RenderToSwapchain({
    .viewport = Fwog::Viewport{.drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}}},
    .colorLoadOp =  Fwog::AttachmentLoadOp::CLEAR,
    .clearColorValue = {.1f, .2f, .4f, 1.0f},
  },
  [&] {
    // Rendering commands
    ...
  });
```

Setting `colorLoadOp` to `CLEAR` means that the image will be cleared when we call `RenderToSwapchain`. In this case, we clear to a soft blue color.

To actually draw something, we need to first bind out pipeline inside this scope. All drawing functions will use the state encapsulated in this pipeline until another pipeline is bound or another rendering scope is begun (in which case we will have to rebind the pipeline).

```cpp
[&] {
  // Rendering commands
  Fwog::Cmd::BindGraphicsPipeline(pipeline);
  ...
}
```

With the pipeline bound, we can finally draw something!

```cpp
[&] {
  // Rendering commands
  Fwog::Cmd::BindGraphicsPipeline(pipeline);
  Fwog::Cmd::Draw(3, 1, 0, 0);
}
```

Of course, without a fragment shader, this won't do anything interesting (depth test & write are disabled by default). For a more complete example, check [01_hello_triangle](https://github.com/JuanDiegoMontoya/Fwog/blob/main/example/01_hello_triangle.cpp). More sophisticated programs showcasing Fwog's features can also be found in the [examples folder](https://github.com/JuanDiegoMontoya/Fwog/tree/main/example).

[Fwog-CMake-Glfw-OpenGL-Template](https://github.com/ClementineAccount/Fwog-CMake-Glfw-OpenGL-Template) is a solid starting point if you would like to start using Fwog immediately.

## Extensions

Fwog natively supports a number of ubiquitous extensions:

- `GL_ARB_bindless_texture`
- `GL_EXT_texture_compression_s3tc`
- `GL_EXT_texture_sRGB`
- `GL_KHR_shader_subgroup`

## Directory Structure

```text
.
├── .github             # GitHub workflows
├── cmake               # Find packages for CMake
├── docs                # Files for building Fwog's docs
├── example             # Sources for example programs using Fwog
│   ├── common          # C++ sources used in multiple examples
│   ├── external        # Build files for fetching third-party libraries
│   ├── media           # Media showcasing the examples
│   ├── models          # Default glTF models
│   ├── shaders         # GLSL shader sources
│   ├── textures        # Common textures
│   └── vendor          # Vendored third-party libraries
├── external            # Build files for fetching third-party libraries
├── include/Fwog        # Public C++ interface
│   └── detail          # Private C++ interface
├── media               # Media used in this readme
├── src                 # Implementation of public C++ interface
│   └── detail          # Implementation of private C++ interface
├── CMakeLists.txt
├── CMakeSettings.json
├── LICENSE
└── README.md
```
