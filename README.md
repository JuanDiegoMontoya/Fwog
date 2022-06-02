# Fwog (previously g)

Low-level OpenGL 4.6 abstraction written in C++20. The abstraction attempts to mitigate the weak points of OpenGL while providing an interface fit for use in modern renderers (outlined in [my blog post about modern OpenGL best practices](https://juandiegomontoya.github.io/modern_opengl.html)).

Fwog is inspired by Vulkan's and D3D12's modern design, while maintaining the simplicity of OpenGL. Compared to raw OpenGL, Fwog has about the same (small) amount of boilerplate required to get started.

The user must bind a pipeline with all the state they intend to use in a draw prior to submitting commands, but certain things, like buffer and texture creation, are simplified. The advantage of the pipeline approach is that it's impossible to leak pipeline state (with normal usage), clarity is improved, and the driver can more easily reason about the program state.

**Fwog is a WIP and should be treated as such**. It is inefficient with redundant state setting, interfaces are not final, and the code is smelly in more than a few places. If you rely on it in its current form, you might croak. Use at your own risk!

## Goals of Fwog

- Reduce potential for error
  - Add type safety
    - Type wrappers with move semantics and no copyability
    - Strongly-typed enums instead of defines
  - Reduce or remove global state (by requiring render passes and pipelines)
  - Sane vertex specification (using vertex format)
  - Automatic object cleanup (RAII)
  - Use C++ standard library features like std::span or std::vector instead of raw pointers
- Ease of use
  - Easier to create and track objects
  - Member functions and namespaces
- Hide problematic OpenGL features
  - No built-in samplers on textures (require usage of sampler objects)
  - No binding to edit (replaced by member functions that internally use DSA)
  - No compatibility profile or "old" features (like glClear)

It may be noted that the API heavily resembles Vulkan. This is intentional.

## <ins>Not</ins> Goals of Fwog

- Backwards compatibility
  - The FFP has no power here!
- High level abstraction (e.g. scene management, frame graph, etc.)
- Window creation and context management
- Supporting all features up to OpenGL 4.6, notably:
  - Scalar, vector, and matrix uniform variables in shaders
    - You must instead use uniform and storage buffers
  - Geometry and tessellation control/evaluation shaders
  - Multisampled rasterization
  - Multiple viewports/layered rendering
  - Transform feedback
  - Hardware occlusion queries

## Core Features

- [x] Rendering info (BeginRendering and EndRendering)
  - [VkRenderingInfo](https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderingInfo.html)
- [x] Graphics pipeline
- [x] Compute pipeline


## API Object Abstractions

- [x] Texture
- [x] Texture view
- [x] Sampler
- [x] Buffer
- [x] Fence sync
- [x] Vertex spec
- [x] Debug group (automatic scope)
- [x] Timer query (synchronous and asynchronous)
- [X] Shader

## Potential Future Features

- [x] Sampler deduplication
- [ ] State deduplication
- [ ] Dynamic state (careful to allow only dynamic state that is free on modern hardware)
- [ ] Context object/global object management
- [ ] Forced driver pipeline compilation to reduce stuttering (issue dummy draw/dispatch when compiling pipelines)
- [ ] Better error handling (maybe even exceptions, hehe)
- [ ] Queue submission of certain commands for multithreading
- [ ] Texture view deduplication

## Build Instructions

```bash
$ git clone https://github.com/JuanDiegoMontoya/Fwog.git
$ mkdir build
$ cd build
$ cmake ..
```

## Example

The draw loop of hello triangle looks like this:

```cpp
Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
Fwog::Cmd::BindGraphicsPipeline(pipeline);
Fwog::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, 2 * sizeof(float));
Fwog::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, 3 * sizeof(uint8_t));
Fwog::Cmd::Draw(3, 1, 0, 0);
Fwog::EndRendering();
```

For more examples, consult the top-level example folder.
