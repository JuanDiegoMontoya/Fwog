# g (working title)
Low-level OpenGL 4.6 abstraction in C++20.

## goals
- reduce potential for error
  - add type safety
    - type wrappers
    - strongly-typed enums instead of defines
  - reduce or remove global state (by requiring render passes and pipelines)
  - sane vertex specification (using vertex format)
  - const correctness
  - use C++ standard library features like std::span or std::vector instead of raw pointers
- remove problematic OpenGL features
  - no built-in samplers on textures (require usage of sampler objects)
  - no binding to edit (replaced by member functions)
  - no compatibility profile or "old" features (like glClear)
- error-safe(?)
  - object creation yields optional type or something (people won't like it if constructors can throw)

## core features
- [ ] texture
- [ ] texture view
- [ ] sampler
- [ ] buffer
- [ ] rendering info
  - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkRenderingInfo.html
- [ ] graphics pipeline
- [ ] compute pipeline
- [ ] fence sync
- [ ] vertex spec/VAO
  - DSA-like specification interface
- [ ] debug groups (manual and automatic scope)
- [ ] framebuffer(?)
  - unnecessary with dynamic rendering
  - ...but how will framebuffer operations be handled? opaque framebuffers?
- [ ] shader(?)

## extended features (which may not come)
- [ ] automatically reflect shader uniforms
- [ ] state deduplication
- [ ] sampler deduplication
- [ ] texture view deduplication
