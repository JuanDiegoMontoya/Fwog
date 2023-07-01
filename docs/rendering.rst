Rendering
=========
Fwog forgoes framebuffers in favor of specifying a list of render targets at draw time, taking inspiration from `VK_KHR_dynamic_rendering <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_dynamic_rendering.html>`_.

Color attachments are bound in the order in which they're provided, which means your fragment shader outputs should use sequential explicit locations starting at zero.

.. code-block:: cpp

    Fwog::RenderColorAttachments colorAttachments[] = {aAlbedo, aNormal, aMaterial};

and the corresponding fragment shader outputs:

.. code-block:: c

    layout(location = 0) out vec3 o_albedo;
    layout(location = 1) out vec3 o_normal;
    layout(location = 2) out vec3 o_material;

To bind the render targets and begin a rendering pass, call :cpp:func:`Fwog::Render`.

.. code-block:: cpp

    Fwog::Render({
        .colorAttachments = colorAttachments,
        .depthAttachment = &depthAttachment,
    }
    [&] {
        // Bind pipelines, resources, and make draw calls here
    });

Then, you can bind pipelines and resources and issue draw calls inside of the callable that is passed.

If you wish to render to the screen, call :cpp:func:`Fwog::RenderToSwapchain`. 

Compute
-------
Compute piplines are similar to graphics pipelines, except they only encapsulate a compute shader. To issue dispatches, call :cpp:func:`Fwog::BeginCompute` to begin a compute scope.

Color Spaces
------------
Fwog enables ``GL_FRAMEBUFFER_SRGB`` by default. :cpp:class:`Fwog::TextureView` can be used to view an image in a different color space if desired. This follows the same rules as `glTextureView <https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTextureView.xhtml>`_.

Synchronization
---------------
Like in plain OpenGL, most operations are automatically synchronized with respect to each other. However, there are certain instances where the driver may not automatically resolve a hazard. These can be dealt with by calling :cpp:func:`Fwog::MemoryBarrier` and :cpp:func:`Fwog::TextureBarrier`. Consult the OpenGL specification for more information.

`#include "Fwog/Rendering.h"`

.. doxygenfile:: Rendering.h