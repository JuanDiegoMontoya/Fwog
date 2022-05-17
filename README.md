# Fwog (previously g)

Low-level OpenGL 4.6 abstraction in C++20. The abstraction attempts to mitigate the weak points of OpenGL while providing an interface fit for use in modern renderers (outlined in [my blog post about modern OpenGL best practices](https://juandiegomontoya.github.io/modern_opengl.html)).

The abstraction is inspired by Vulkan in many places while requiring considerably less boilerplate. That being said, this API *adds* boilerplate compared to plain OpenGL as the user **must** explitly state all graphics pipeline state before rendering. The advantage of doing so is that it's impossible to leak pipeline state (with normal usage) and clarity is greatly improved.

**Currently this is a proof of concept and should be treated as such**. The abstraction is inefficient with redundant state setting, interfaces are not final, and the code is smelly in more than a few places.

## Goals

- Reduce potential for error
  - Add type safety
    - Type wrappers with move semantics and no copyability
    - Strongly-typed enums instead of defines
  - Reduce or remove global state (by requiring render passes and pipelines)
  - Sane vertex specification (using vertex format)
  - Automatic object cleanup (RAII)
  - Use C++ standard library features like std::span or std::vector instead of raw pointers
- Ease of use
  - Easier to create objects and track them
  - Member functions and namespaces
- Hide problematic OpenGL features
  - No built-in samplers on textures (require usage of sampler objects)
  - No binding to edit (replaced by member functions that internally use DSA)
  - No compatibility profile or "old" features (like glClear)
- Error-safe(?)
  - Object creation yields optional type or something (people won't like it if constructors can throw)

It may be noted that the API heavily resembles Vulkan. This is intentional.

## Not Goals

- Backwards compatibility
- High level abstraction (e.g. scene management, frame graph, etc.)
- Supporting all OpenGL 4.6 features

## Core Features

- [x] Texture
- [x] Texture view
- [x] Sampler
- [x] Buffer
- [x] Rendering info (BeginRendering and EndRendering)
  - [VkRenderingInfo](https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderingInfo.html)
- [x] Graphics pipeline
- [x] Compute pipeline
- [x] Fence sync
- [x] Vertex spec
- [x] Debug groups (automatic scope)
- [x] Timer query (synchronous and asynchronous)
- [ ] Shader (this is basically bundled with pipelines so I'm not sure what to do here)

## Extended Features (which may not come)

- [ ] Automatically reflect shader uniforms
- [ ] State deduplication
- [x] Sampler deduplication
- [ ] Texture view deduplication

## Build Instructions

```bash
$ git clone https://github.com/JuanDiegoMontoya/g.git
$ mkdir build
$ cd build
$ cmake ..
```

## Example

The draw loop of hello triangle looks like this (WIP):

```cpp
Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
Fwog::Cmd::BindGraphicsPipeline(pipeline);
Fwog::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, 2 * sizeof(float));
Fwog::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, 3 * sizeof(uint8_t));
Fwog::Cmd::Draw(3, 1, 0, 0);
Fwog::EndRendering();
```
