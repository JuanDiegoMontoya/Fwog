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

## extended features (which may not come)
- [ ] automatically reflect shader uniforms
- [ ] state deduplication
- [ ] sampler deduplication
- [ ] texture view deduplication
