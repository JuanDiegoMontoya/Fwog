Getting Started
===============
To install Fwog, run the following commands in your friendly neighborhood terminal:

.. code-block:: bash

    git clone https://github.com/JuanDiegoMontoya/Fwog.git
    cd Fwog
    mkdir build
    cd build
    cmake ..

Creating a Fwog
---------------
To create a Fwog (context), you will first need an OpenGL 4.6 context (e.g., one created with GLFW) and have loaded OpenGL function pointers in the appropriate location. Then, you can call :cpp:func:`Fwog::Initialize`.

`#include "Fwog/Context.h"`

.. doxygenfile:: Context.h
    :sections: func innernamespace

Debugging
---------
Fwog helps you debug by enabling certain features in debug builds (unless overridden).

First, assert macros are generously sprinkled throughout the source to catch programming bugs. These will catch things like trying to clear a render target with an incompatible clear color or trying to call rendering commands outside of a rendering scope.

Fwog also clears all resource bindings at the end of each pass to ensure there is no reliance on leaked state.

These debugging features are disabled in release builds to ensure maximum performance, but it also means the programming bugs they catch will result in undefined behavior rather than an instant crash.

Configuring
-----------
Fwog can be configured by adding certain defines to your compile command:

- ``FWOG_FORCE_DEBUG``: If defined, debug functionality will be enabled in all builds.
- ``FWOG_ASSERT <assert-like-construct>``: Defines a custom assert function/macro for Fwog to use internally. By default, Fwog will use ``assert``.
- ``FWOG_UNREACHABLE <unreachable-like-construct>``: Defines a custom unreachable function/macro for Fwog to use internally. By default, Fwog will simply use ``FWOG_ASSERT(0)`` for unreachable paths.
- ``FWOG_OPENGL_HEADER <header-string>``: Allows the user to define where OpenGL function declarations can be found. By default, Fwog will search for ``<glad/gl.h>``.
- ``FWOG_DEFAULT_CLIP_DEPTH_RANGE_ZERO_TO_ONE``: If defined, the default value for Viewport::depthRange will be :cpp:enumerator:`Fwog::ClipDepthRange::ZeroToOne`. Otherwise, its default value will be :cpp:enumerator:`Fwog::ClipDepthRange::NegativeOneToOne`.

Errors
------
Fwog uses exceptions to propagate errors. Currently, exceptions are only used in a few places.

.. doxygenfile:: Exception.h

Device Properties
-----------------
Sometimes, it can be useful to query some information about the device. Fwog provides :cpp:func:`Fwog::GetDeviceProperties` to query the following information:

.. doxygenstruct:: Fwog::DeviceLimits
    :members:
    :undoc-members:

.. doxygenstruct:: Fwog::DeviceFeatures
    :members:
    :undoc-members:

.. doxygenstruct:: Fwog::DeviceProperties
    :members:
    :undoc-members: