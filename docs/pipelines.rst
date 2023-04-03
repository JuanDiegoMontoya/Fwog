Pipelines
=========
Pipeline objects are a concept that are familiar to Vulkan or D3D12 users, but new to OpenGL users. Essentially, they allow us to specify the entire state of the graphics pipeline (shaders, vertex input, blending, depth stencil, etc.) up-front.

To use a pipeline, we first bind it in a rendering scope. All draw calls issued thereafter will use the state encapsulated in the pipeline.

.. code-block:: cpp

    Fwog::BeginRendering(...);
    Fwog::Cmd::BindGraphicsPipeline(fooPipeline);
    Fwog::Cmd::Draw(...); // Will use the state from fooPipeline
    Fwog::Cmd::BindGraphicsPipeline(barPipeline);
    Fwog::Cmd::Draw(...); // Will use the state from barPipeline
    Fwog::EndRendering();

Note that it's illegal to issue a draw in a rendering scope without first binding a pipeline. This means you cannot rely on stale bindings from other scopes.

Under the Hood
--------------
Internally, Fwog tracks relevant OpenGL state to ensure that binding pipelines won't set redundant state. Pipeline binding will only incur the cost of setting the difference between that pipeline and the previous (and the cost to find the difference).

`#include "Fwog/Pipeline.h"`

.. doxygenfile:: Pipeline.h