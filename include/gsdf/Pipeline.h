#pragma once
#include <gsdf/BasicTypes.h>
#include <gsdf/Flags.h>
#include <span>

namespace GFX
{
  enum class PrimitiveTopology
  {
    POINT_LIST,
    LINE_LIST,
    LINE_STRIP,
    TRIANGLE_LIST,
    TRIANGLE_STRIP,
    TRIANGLE_FAN,
    // TODO: add more toplogies that are deemed useful
  };

  struct InputAssemblyState
  {
    PrimitiveTopology topology;
    bool primitiveRestartEnable;
  };

  enum class PolygonMode
  {
    FILL,
    LINE,
    POINT,
  };

  enum class CullModeBits
  {
    FRONT          = 0b01,
    BACK           = 0b10,
    FRONT_AND_BACK = 0b11,
  };
  DECLARE_FLAG_TYPE(CullMode, CullModeBits, uint32_t)

  enum class FrontFace
  {
    CLOCKWISE,
    COUNTERCLOCKWISE,
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

  enum class CompareOp
  {
    NEVER,
    LESS,
    EQUAL,
    LESS_OR_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_OR_EQUAL,
    ALWAYS,
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

  enum class LogicOp
  {
    CLEAR,
    SET,
    COPY,
    COPY_INVERTED,
    NO_OP,
    INVERT,
    AND,
    NAND,
    OR,
    NOR,
    XOR,
    EQUIVALENT,
    AND_REVERSE,
    OR_REVERSE,
    AND_INVERTED,
    OR_INVERTED,
  };

  enum class BlendFactor
  {
    ZERO,
    ONE,
    SRC_COLOR,
    ONE_MINUS_SRC_COLOR,
    DST_COLOR,
    ONE_MINUS_DST_COLOR,
    SRC_ALPHA,
    ONE_MINUS_SRC_ALPHA,
    DST_ALPHA,
    ONE_MINUS_DST_ALPHA,
    CONSTANT_COLOR,
    ONE_MINUS_CONSTANT_COLOR,
    CONSTANT_ALPHA,
    ONE_MINUS_CONSTANT_ALPHA,
    SRC_ALPHA_SATURATE,
    SRC1_COLOR,
    ONE_MINUS_SRC1_COLOR,
    SRC1_ALPHA,
    ONE_MINUS_SRC1_ALPHA,
  };

  enum class BlendOp
  {
    ADD,
    SUBTRACT,
    REVERSE_SUBTRACT,
    MIN,
    MAX,
  };

  enum class ColorComponentFlagsBits
  {
    R_BIT = 0b0001,
    G_BIT = 0b0010,
    B_BIT = 0b0100,
    A_BIT = 0b1000,
  };
  DECLARE_FLAG_TYPE(ColorComponentFlags, ColorComponentFlagsBits, uint32_t)

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