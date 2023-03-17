# Fwog

![Fwog logo](media/logo.png)

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
- Geometry and tessellation control/evaluation shaders
  - Alternative: compute shaders
- Multisampled rasterization
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

To use the pipeline, we need to be in a rendering scope. To do this, we need to call `BeginRendering` (to render to off-screen render targets) or `BeginSwapchainRendering` (to render directly to the screen).

```cpp
Fwog::BeginSwapchainRendering({
  .viewport = Fwog::Viewport{.drawRect{.offset = {0, 0}, .extent = {windowWidth, windowHeight}}},
  .colorLoadOp =  Fwog::AttachmentLoadOp::CLEAR,
  .clearColorValue = {.1f, .2f, .4f, 1.0f},
});
```

Setting `colorLoadOp` to `CLEAR` means that the image will be cleared when we call `BeginSwapchainRendering`. In this case, we clear to a soft blue color.

To actually draw something, we need to first bind out pipeline inside this scope. All drawing functions will use the state encapsulated in this pipeline until another pipeline is bound or another rendering scope is begun (in which case we will have to rebind the pipeline).

```cpp
Fwog::Cmd::BindGraphicsPipeline(pipeline);
```

With the pipeline bound, we can finally draw something!

```cpp
Fwog::Cmd::Draw(3, 1, 0, 0);
```

Of course, without a fragment shader, this won't do anything (depth test & write are disabled by default). For a more complete example, check [01_hello_triangle](https://github.com/JuanDiegoMontoya/Fwog/blob/main/example/01_hello_triangle.cpp).

To finish things, we need to end the rendering scope. Doing so will demarcate where `Fwog::Cmd`s can be called (they can only be called inside rendering/compute scopes) and allow us to later start a new rendering scope.

```cpp
Fwog::EndRendering();
```

Consult the [example](https://github.com/JuanDiegoMontoya/Fwog/tree/main/example) folder for less-trivial programs that use Fwog.
