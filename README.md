# g (working title)
Low-level OpenGL 4.6 abstraction in C++20.

## Goals
- reduce potential for error
  - add type safety
    - type wrappers with move semantics and no copyability
    - strongly-typed enums instead of defines
  - reduce or remove global state (by requiring render passes and pipelines)
  - sane vertex specification (using vertex format)
  - automatic object cleanup (RAII)
  - use C++ standard library features like std::span or std::vector instead of raw pointers
- ease of use
  - easier to create objects and track them
  - member functions and namespaces
- remove problematic OpenGL features
  - no built-in samplers on textures (require usage of sampler objects)
  - no binding to edit (replaced by member functions)
  - no compatibility profile or "old" features (like glClear)
- error-safe(?)
  - object creation yields optional type or something (people won't like it if constructors can throw)

It may be noted that the API heavily resembles Vulkan. This is intentional.

## Not Goals
- backwards compatibility
- high level abstraction (e.g. scene management, frame graph, etc.)
- supporting all OpenGL 4.6 features

## Core Features
- [x] texture
- [x] texture view
- [x] sampler
- [x] buffer
- [x] rendering info (BeginRendering and EndRendering)
  - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderingInfo.html
- [x] graphics pipeline
- [ ] compute pipeline
- [ ] fence sync
- [x] vertex spec/VAO
- [ ] debug groups (manual and automatic scope)
- [ ] shader(?)

## Extended Features (which may not come)
- [ ] automatically reflect shader uniforms
- [ ] state deduplication
- [ ] sampler deduplication
- [ ] texture view deduplication

## Example
The draw loop of hello triangle looks like this (WIP):
```cpp
GFX::BeginSwapchainRendering(swapchainRenderingInfo);
GFX::Cmd::BindPipeline(pipeline);
GFX::Cmd::BindVertexBuffer(0, *vertexPosBuffer, 0, 2 * sizeof(float));
GFX::Cmd::BindVertexBuffer(1, *vertexColorBuffer, 0, 3 * sizeof(uint8_t));
GFX::Cmd::Draw(3, 1, 0, 0);
GFX::EndRendering();
```
