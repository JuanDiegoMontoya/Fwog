Welcome to Fwog's documentation!
================================

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   basics
   pipelines
   rendering
   reference

Summary
-------
Fwog is a low-level OpenGL 4.6 abstraction written in C++20 which aims to offer improved ergonomics and eliminate deprecated functionality. The goal is to wrap OpenGL in an expressive, type-safe, and easy-to-use wrapper.

The design of Fwog is motivated by some of the shortcomings of OpenGL that I identified `in my blog post <https://juandiegomontoya.github.io/modern_opengl.html>`_. For example, Fwog wraps OpenGL's integer constants with strongly-typed enumerations. Fwog also offers RAII wrappers for most OpenGL object types (buffers, textures, shaders, fences, and timer queries). Other types, like vertex arrays and framebuffers, are completely abstracted away in favor of a cleaner programming model.

Fwog uses pipeline objects to encapsulate what would otherwise be global OpenGL state. These objects are bound before drawing to set the state of the pipeline, making it clear to the reader which state will be used and preventing subtle bugs where state leaks from one pass to the next. Pipelines also offer the potential benefit of reduced stuttering by providing more information to the driver up-front (`source <https://developer.nvidia.com/opengl-vulkan>`_).

Caveats
-------
Given Fwog's goals, there are a number of features that the API does not expose:

- The default uniform block (i.e., uniforms set with ``glUniform*``)
- Geometry shaders
- Multi-viewport/layered rendering
- Multisampled rasterization
- Transform feedback
- Hardware occlusion queries
- SPIR-V shaders
- All deprecated OpenGL features

Some of these (such as multisampled rasterization and geo/tess shaders) have not been implemented due to a lack of perceived interest, while others (such as ``glUniform`` and transform feedback) aren't due to their orthogonality to Fwog's design. Please raise an issue if you would like a feature to be implemented. Otherwise, it's possible to use certain unsupported features alongside Fwog's abstraction.

Indices and tables
==================

* :ref:`genindex`