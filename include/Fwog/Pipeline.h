#pragma once
#include <Fwog/Config.h>
#include <Fwog/BasicTypes.h>
#include <Fwog/detail/Flags.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace Fwog
{
  // clang-format off
  class Shader;

  struct InputAssemblyState
  {
    PrimitiveTopology topology  = PrimitiveTopology::TRIANGLE_LIST;
    bool primitiveRestartEnable = false;
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
    std::span<const VertexInputBindingDescription> vertexBindingDescriptions = {};
  };

  struct TessellationState
  {
    uint32_t patchControlPoints; // glPatchParameteri(GL_PATCH_VERTICES, ...)
  };

  struct RasterizationState
  {
    bool depthClampEnable         = false;
    PolygonMode polygonMode       = PolygonMode::FILL;
    CullMode cullMode             = CullMode::BACK;
    FrontFace frontFace           = FrontFace::COUNTERCLOCKWISE;
    bool depthBiasEnable          = false;
    float depthBiasConstantFactor = 0;
    float depthBiasSlopeFactor    = 0;
    float lineWidth               = 1; // glLineWidth
    float pointSize               = 1; // glPointSize
  };

  struct MultisampleState
  {
    bool sampleShadingEnable   = false;      // glEnable(GL_SAMPLE_SHADING)
    float minSampleShading     = 1;          // glMinSampleShading
    uint32_t sampleMask        = 0xFFFFFFFF; // glSampleMaski
    bool alphaToCoverageEnable = false;      // glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)
    bool alphaToOneEnable      = false;      // glEnable(GL_SAMPLE_ALPHA_TO_ONE)
};

  struct DepthState
  {
    bool depthTestEnable     = false;           // gl{Enable, Disable}(GL_DEPTH_TEST)
    bool depthWriteEnable    = false;           // glDepthMask(depthWriteEnable)
    CompareOp depthCompareOp = CompareOp::LESS; // glDepthFunc
  };

  struct StencilOpState
  {
    StencilOp passOp      = StencilOp::KEEP;   // glStencilOp (dppass)
    StencilOp failOp      = StencilOp::KEEP;   // glStencilOp (sfail)
    StencilOp depthFailOp = StencilOp::KEEP;   // glStencilOp (dpfail)
    CompareOp compareOp   = CompareOp::ALWAYS; // glStencilFunc (func)
    uint32_t compareMask  = 0;                 // glStencilFunc (mask)
    uint32_t writeMask    = 0;                 // glStencilMask
    uint32_t reference    = 0;                 // glStencilFunc (ref)

    bool operator==(const StencilOpState&) const noexcept = default;
  };

  struct StencilState
  {
    bool stencilTestEnable = false;
    StencilOpState front   = {};
    StencilOpState back    = {};
  };

  struct ColorBlendAttachmentState                                      // glBlendFuncSeparatei + glBlendEquationSeparatei
  {
    bool blendEnable = false;                                           // if false, blend factor = one?
    BlendFactor srcColorBlendFactor    = BlendFactor::ONE;              // srcRGB
    BlendFactor dstColorBlendFactor    = BlendFactor::ZERO;             // dstRGB
    BlendOp colorBlendOp               = BlendOp::ADD;                  // modeRGB
    BlendFactor srcAlphaBlendFactor    = BlendFactor::ONE;              // srcAlpha
    BlendFactor dstAlphaBlendFactor    = BlendFactor::ZERO;             // dstAlpha
    BlendOp alphaBlendOp               = BlendOp::ADD;                  // modeAlpha
    ColorComponentFlags colorWriteMask = ColorComponentFlag::RGBA_BITS; // glColorMaski

    bool operator==(const ColorBlendAttachmentState&) const noexcept = default;
  };

  struct ColorBlendState
  {
    bool logicOpEnable                                     = false;          // gl{Enable, Disable}(GL_COLOR_LOGIC_OP)
    LogicOp logicOp                                        = LogicOp::COPY;  // glLogicOp(logicOp)
    std::span<const ColorBlendAttachmentState> attachments = {};             // glBlendFuncSeparatei + glBlendEquationSeparatei
    float blendConstants[4]                                = { 0, 0, 0, 0 }; // glBlendColor
  };

  /// @brief Parameters for the constructor of GraphicsPipeline
  struct GraphicsPipelineInfo
  {
    /// @brief An optional name for viewing in a graphics debugger
    std::string_view name;

    /// @brief Non-null pointer to a vertex shader
    const Shader* vertexShader            = nullptr;

    /// @brief Optional pointer to a fragment shader
    const Shader* fragmentShader          = nullptr;

    /// @brief Optional pointer to a tessellation control shader
    const Shader* tessellationControlShader = nullptr;

    /// @brief Optional pointer to a tessellation evaluation shader
    const Shader* tessellationEvaluationShader = nullptr;

    InputAssemblyState inputAssemblyState = {};
    VertexInputState vertexInputState     = {};
    TessellationState tessellationState   = {};
    RasterizationState rasterizationState = {};
    MultisampleState multisampleState     = {};
    DepthState depthState                 = {};
    StencilState stencilState             = {};
    ColorBlendState colorBlendState       = {};
  };

  /// @brief Parameters for the constructor of ComputePipeline
  struct ComputePipelineInfo
  {
    /// @brief An optional name for viewing in a graphics debugger
    std::string_view name;

    /// @brief Non-null pointer to a compute shader
    const Shader* shader;
  };

  /// @brief An object that encapsulates the state needed to issue draws
  struct GraphicsPipeline
  {
    /// @throws PipelineCompilationException
    explicit GraphicsPipeline(const GraphicsPipelineInfo& info);
    ~GraphicsPipeline();
    GraphicsPipeline(GraphicsPipeline&& old) noexcept;
    GraphicsPipeline& operator=(GraphicsPipeline&& old) noexcept;
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    bool operator==(const GraphicsPipeline&) const = default;

    /// @brief Gets the handle of the underlying OpenGL program object
    /// @return The program
    [[nodiscard]] uint64_t Handle() const
    {
      return id_;
    }

  private:
    uint64_t id_;
  };

  /// @brief An object that encapsulates the state needed to issue dispatches
  struct ComputePipeline
  {
    /// @throws PipelineCompilationException
    explicit ComputePipeline(const ComputePipelineInfo& info);
    ~ComputePipeline();
    ComputePipeline(ComputePipeline&& old) noexcept;
    ComputePipeline& operator=(ComputePipeline&& old) noexcept;
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    bool operator==(const ComputePipeline&) const = default;
    
    [[nodiscard]] Extent3D WorkgroupSize() const;
    
    /// @brief Gets the handle of the underlying OpenGL program object
    /// @return The program
    [[nodiscard]] uint64_t Handle() const
    {
      return id_;
    }

  private:
    uint64_t id_;
  };

  // clang-format on
} // namespace Fwog