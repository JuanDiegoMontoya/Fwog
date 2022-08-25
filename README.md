# Fwog

![Fwog logo](media/logo.png)

## Froggy OpenGL Engoodener

Low-level OpenGL 4.6 abstraction written in C++20. Fwog mitigates many of the weak points of OpenGL while providing a modern interface (outlined in [my blog post](https://juandiegomontoya.github.io/modern_opengl.html)).

**Fwog is a WIP and should be treated as such**. Expect everything to break frequently.

## Core Features

- Type-safe RAII wrappers for most API objects (buffers, textures, samplers, fences, and more)
- Type-safe enums and bit flags
- Simplified render target specification
- Coalesce global pipeline state into pipeline objects
- Prevent state leaking bugs by using clearly-marked rendering scopes

## Divergence from OpenGL

- No ability to set individual uniforms
  - Alternative: uniform and storage buffers
- No geometry and tessellation control/evaluation shaders
  - Alternative: compute shaders
- No multisampled rasterization
- No multiple viewports/layered rendering
- No transform feedback
  - Alternative: storage buffers
- No hardware occlusion queries
  - Alternative: storage buffer + fence + readback
- ...and probably many more features are not exposed

If an issue is raised about a missing feature, I might add it. If a PR is made that implements it, I will probably merge it.

## Potential Future Features

- [x] Sampler deduplication
- [x] State deduplication
- [x] Automated debug marker placement
- [ ] Dynamic state (careful to allow only dynamic state that is free to change on modern hardware)
- [ ] Forced driver pipeline compilation to reduce stuttering (issue dummy draw/dispatch when compiling pipelines)
- [ ] Queue submission of certain commands for multithreading
- [ ] Texture view deduplication

## Installing and Building

```bash
$ git clone https://github.com/JuanDiegoMontoya/Fwog.git
$ cd Fwog
$ mkdir build
$ cd build
$ cmake ..
```

## Example

The draw loop of hello triangle looks like this:

```cpp
Fwog::BeginSwapchainRendering(swapchainRenderingInfo);
Fwog::Cmd::BindGraphicsPipeline(pipeline);
Fwog::Cmd::BindVertexBuffer(0, vertexPosBuffer, 0, 2 * sizeof(float));
Fwog::Cmd::BindVertexBuffer(1, vertexColorBuffer, 0, 3 * sizeof(uint8_t));
Fwog::Cmd::Draw(3, 1, 0, 0);
Fwog::EndRendering();
```

For more examples, consult the top-level example folder.
