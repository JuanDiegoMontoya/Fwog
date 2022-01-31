#pragma once
#include <gsdf/BasicTypes.h>
#include <gsdf/Flags.h>
#include <span>

namespace GFX
{
  struct VertexInputBindingDescription
  {
    uint32_t location; // glEnableVertexArrayAttrib + glVertexArrayAttribFormat
    uint32_t binding;  // glVertexArrayAttribBinding
    Format format;     // glVertexArrayAttribFormat
  };

  struct VertexInputState
  {
    std::span<VertexInputBindingDescription> vertexBindingDescriptions;
  };

  struct InputAssemblyState
  {
    PrimitiveTopology topology;
    bool primitiveRestartEnable;
  };

  // TODO: see what rasterization state can be dynamic instead
  struct RasterizationState
  {
    bool depthClampEnable;
    PolygonMode polygonMode;
    CullMode cullMode;
    FrontFace frontFace;
    bool depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasSlopeFactor;
    float depthBiasClamp;
    float lineWidth;
    float pointSize;
  };

  struct DepthStencilState
  {
    bool depthTestEnable;       // gl{Enable, Disable}(GL_DEPTH_TEST)
    bool depthWriteEnable;      // glDepthMask(depthWriteEnable)
    bool depthBoundsTestEnable; // ???
    float minDepthBounds;       // ???
    float maxDepthBounds;       // ???
    // TODO: add stencil stuff here (front and back stencil op)
  };

  struct ColorBlendAttachmentState      // glBlendFuncSeparatei + glBlendEquationSeparatei
  {
    bool blendEnable;                   // if false, blend factor = one?
    BlendFactor srcColorBlendFactor;    // srcRGB
    BlendFactor dstColorBlendFactor;    // dstRGB
    BlendOp colorBlendOp;               // modeRGB
    BlendFactor srcAlphaBlendFactor;    // srcAlpha
    BlendFactor dstAlphaBlendFactor;    // dstAlpha
    BlendOp alphaBlendOp;               // modeAlpha
    ColorComponentFlags colorWriteMask; // glColorMask
  };

  struct ColorBlendState
  {
    bool logicOpEnable;                               // gl{Enable, Disable}(GL_COLOR_LOGIC_OP)
    LogicOp logicOp;                                  // glLogicOp(logicOp)
    std::span<ColorBlendAttachmentState> attachments;
    float blendConstants[4];                          // glBlendColor
  };

  struct GraphicsPipelineInfo
  {
    InputAssemblyState inputAssemblyState;
    RasterizationState rasterizationState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
    // vertex input omitted (tentatively dynamic state)
    // Viewport state omitted (dynamic state)
    // Multisample state omitted (stretch goal)
    // Tessellation state omitted (stretch goal)
  };
}