#pragma once
#include <gsdf/BasicTypes.h>
#include <gsdf/detail/Flags.h>
#include <span>
#include <optional>

namespace GFX
{
  struct InputAssemblyState
  {
    PrimitiveTopology topology;
    bool primitiveRestartEnable;
  };

  struct VertexInputBindingDescription
  {
    uint32_t location; // glEnableVertexArrayAttrib + glVertexArrayAttribFormat
    uint32_t binding;  // glVertexArrayAttribBinding
    Format format;     // glVertexArrayAttribFormat
    uint32_t offset;   // glVertexArrayAttribFormat
  };

  struct VertexInputState
  {
    std::span<const VertexInputBindingDescription> vertexBindingDescriptions;
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
    //float depthBiasClamp; // no equivalent core OpenGL function
    float lineWidth; // glLineWidth
    float pointSize; // glPointSize
  };

  struct DepthStencilState
  {
    bool depthTestEnable;       // gl{Enable, Disable}(GL_DEPTH_TEST)
    bool depthWriteEnable;      // glDepthMask(depthWriteEnable)
    CompareOp depthCompareOp;   // glDepthFunc
    //bool depthBoundsTestEnable; // no equivalent core OpenGL function
    //float minDepthBounds;       // ???
    //float maxDepthBounds;       // ???
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
    ColorComponentFlags colorWriteMask; // glColorMaski
  };

  struct ColorBlendState
  {
    bool logicOpEnable;                               // gl{Enable, Disable}(GL_COLOR_LOGIC_OP)
    LogicOp logicOp;                                  // glLogicOp(logicOp)
    std::span<const ColorBlendAttachmentState> attachments;
    float blendConstants[4];                          // glBlendColor
  };

  struct GraphicsPipelineInfo
  {
    uint32_t shaderProgram; // TODO: temp
    InputAssemblyState inputAssemblyState;
    VertexInputState vertexInputState;
    RasterizationState rasterizationState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
    // vertex input omitted (tentatively dynamic state)
    // Viewport state omitted (dynamic state)
    // Multisample state omitted (stretch goal)
    // Tessellation state omitted (stretch goal)
  };

  struct ComputePipelineInfo
  {
    uint32_t shaderProgram; // TODO: temp
  };

  struct GraphicsPipeline
  {
    auto operator<=>(const GraphicsPipeline&) const = default;
    uint64_t id;
  };
  struct ComputePipeline
  {
    auto operator<=>(const ComputePipeline&) const = default;
    uint64_t id;
  };

  std::optional<GraphicsPipeline> CompileGraphicsPipeline(const GraphicsPipelineInfo& info);
  bool DestroyGraphicsPipeline(GraphicsPipeline pipeline);

  std::optional<ComputePipeline> CompileComputePipeline(const ComputePipelineInfo& info);
  bool DestroyComputePipeline(ComputePipeline pipeline);
}