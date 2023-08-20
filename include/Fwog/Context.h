#pragma once
#include <Fwog/BasicTypes.h>
#include <Fwog/Config.h>
#include <Fwog/Rendering.h>

#include <string_view>
#include <functional>

namespace Fwog
{
  struct SubgroupLimits
  {
    int32_t subgroupSize{}; // GL_SUBGROUP_SIZE_KHR

    // Shader stage support
    bool vertexShaderSupported{};                 // GL_VERTEX_SHADER_BIT
    bool tessellationControlShaderSupported{};    // GL_TESS_CONTROL_SHADER_BIT
    bool tessellationEvaluationShaderSupported{}; // GL_TESS_EVALUATION_SHADER_BIT
    bool fragmentShaderSupported{};               // GL_FRAGMENT_SHADER_BIT
    bool computeShaderSupported{};                // GL_COMPUTE_SHADER_BIT

    // Features
    bool voteSupported{};            // GL_SUBGROUP_FEATURE_VOTE_BIT_KHR
    bool arithmeticSupported{};      // GL_SUBGROUP_FEATURE_ARITHMETIC_BIT_KHR
    bool ballotSupported{};          // GL_SUBGROUP_FEATURE_BALLOT_BIT_KHR
    bool shuffleSupported{};         // GL_SUBGROUP_FEATURE_SHUFFLE_BIT_KHR
    bool shuffleRelativeSupported{}; // GL_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT_KHR
    bool clusteredSupported{};       // GL_SUBGROUP_FEATURE_CLUSTERED_BIT_KHR
    bool quadSupported{};            // GL_SUBGROUP_FEATURE_QUAD_BIT_KHR
  };

  struct DeviceLimits
  {
    int32_t maxTextureSize;     // GL_MAX_TEXTURE_SIZE
    int32_t maxTextureSize3D;   // GL_MAX_3D_TEXTURE_SIZE
    int32_t maxTextureSizeCube; // GL_MAX_CUBE_MAP_TEXTURE_SIZE

    float maxSamplerLodBias;       // GL_MAX_TEXTURE_LOD_BIAS
    float maxSamplerAnisotropy;    // GL_MAX_TEXTURE_MAX_ANISOTROPY
    int32_t maxArrayTextureLayers; // GL_MAX_ARRAY_TEXTURE_LAYERS
    int32_t maxViewportDims[2];    // GL_MAX_VIEWPORT_DIMS
    int32_t subpixelBits;          // GL_SUBPIXEL_BITS
    // int32_t maxClipPlanes;

    int32_t maxFramebufferWidth;  // GL_MAX_FRAMEBUFFER_WIDTH
    int32_t maxFramebufferHeight; // GL_MAX_FRAMEBUFFER_HEIGHT
    int32_t maxFramebufferLayers; // GL_MAX_FRAMEBUFFER_LAYERS
    int32_t maxFramebufferSamples; // GL_MAX_FRAMEBUFFER_SAMPLES
    int32_t maxColorAttachments;  // GL_MAX_COLOR_ATTACHMENTS
    int32_t maxSamples;              // GL_MAX_SAMPLES
    int32_t maxSamplesNoAttachments; // GL_MAX_FRAMEBUFFER_SAMPLES

    float interpolationOffsetRange[2]; // GL_MIN_FRAGMENT_INTERPOLATION_OFFSET & GL_MAX_FRAGMENT_INTERPOLATION_OFFSET
    float pointSizeGranularity;        // GL_POINT_SIZE_GRANULARITY
    float pointSizeRange[2];           // GL_POINT_SIZE_RANGE
    float lineWidthRange[2];           // GL_ALIASED_LINE_WIDTH_RANGE

    int32_t maxElementIndex;               // GL_MAX_ELEMENT_INDEX
    int32_t maxVertexAttribs;              // GL_MAX_VERTEX_ATTRIBS
    int32_t maxVertexAttribBindings;       // GL_MAX_VERTEX_ATTRIB_BINDINGS
    int32_t maxVertexAttribStride;         // GL_MAX_VERTEX_ATTRIB_STRIDE
    int32_t maxVertexAttribRelativeOffset; // GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET
    int32_t maxVertexOutputComponents;     // GL_MAX_VERTEX_OUTPUT_COMPONENTS
    int32_t maxTessellationControlPerVertexInputComponents; // GL_MAX_TESS_CONTROL_INPUT_COMPONENTS
    int32_t maxTessellationControlPerVertexOutputComponents;// GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS
    int32_t maxTessellationControlPerPatchOutputComponents; // GL_MAX_TESS_PATCH_COMPONENTS
    int32_t maxTessellationControlTotalOutputComponents;    // GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS
    int32_t maxTessellationEvaluationInputComponents;       // GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS
    int32_t maxTessellationEvaluationOutputComponents;      // GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS
    int32_t maxFragmentInputComponents;    // GL_MAX_FRAGMENT_INPUT_COMPONENTS
    int32_t texelOffsetRange[2];           // GL_MIN_PROGRAM_TEXEL_OFFSET & GL_MAX_PROGRAM_TEXEL_OFFSET
    int32_t textureGatherOffsetRange[2];   // GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET & GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET

    int32_t maxTessellationGenerationLevel; // GL_MAX_TESS_GEN_LEVEL
    int32_t maxPatchSize;                   // GL_MAX_PATCH_VERTICES

    int32_t maxUniformBufferBindings;     // GL_MAX_UNIFORM_BUFFER_BINDINGS
    int32_t maxUniformBlockSize;          // GL_MAX_UNIFORM_BLOCK_SIZE
    int32_t uniformBufferOffsetAlignment; // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
    int32_t maxCombinedUniformBlocks;     // GL_MAX_COMBINED_UNIFORM_BLOCKS

    int32_t maxShaderStorageBufferBindings;     // GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
    int32_t maxShaderStorageBlockSize;          // GL_MAX_SHADER_STORAGE_BLOCK_SIZE
    int32_t shaderStorageBufferOffsetAlignment; // GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT
    int32_t maxCombinedShaderStorageBlocks;     // GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS

    int32_t maxCombinedShaderOutputResources; // GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES
    int32_t maxCombinedTextureImageUnits;     // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS

    // int32_t maxTextureBufferSize; // GL_MAX_TEXTURE_BUFFER_SIZE

    int32_t maxComputeSharedMemorySize;     // GL_MAX_COMPUTE_SHARED_MEMORY_SIZE
    int32_t maxComputeWorkGroupInvocations; // GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
    int32_t maxComputeWorkGroupCount[3];    // GL_MAX_COMPUTE_WORK_GROUP_COUNT
    int32_t maxComputeWorkGroupSize[3];     // GL_MAX_COMPUTE_WORK_GROUP_SIZE

    int32_t maxImageUnits;                      // GL_MAX_IMAGE_UNITS
    int32_t maxFragmentCombinedOutputResources; // GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS
    int32_t maxCombinedImageUniforms;           // GL_MAX_COMBINED_IMAGE_UNIFORMS
    int32_t maxServerWaitTimeout;               // GL_MAX_SERVER_WAIT_TIMEOUT

    SubgroupLimits subgroupLimits{};
  };

  struct DeviceFeatures
  {
    bool bindlessTextures{}; // GL_ARB_bindless_texture
    bool shaderSubgroup{}; // GL_KHR_shader_subgroup
  };

  struct DeviceProperties
  {
    std::string_view vendor;
    std::string_view renderer;
    std::string_view version;
    std::string_view shadingLanguageVersion;
    int32_t glVersionMajor; // GL_MAJOR_VERSION
    int32_t glVersionMinor; // GL_MINOR_VERSION
    DeviceLimits limits;
    DeviceFeatures features;
  };

  struct ContextInitializeInfo
  {
    /// @brief Callback for logging verbose messages about Fwog's internal state.
    /// Currently, only OpenGL object creation and destruction are logged.
    /// This callback can be useful for analyzing how Fwog implicitly creates objects.
    void (*verboseMessageCallback)(std::string_view message) = nullptr;

    /// @brief Profiling hooks. Note that you are responsible for calling func here if you use the hook!
    void (*renderToSwapchainHook)(const SwapchainRenderInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*renderHook)(const RenderInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*renderNoAttachmentsHook)(const RenderNoAttachmentsInfo& renderInfo, const std::function<void()>& func) = nullptr;
    void (*computeHook)(std::string_view name, const std::function<void()>& func) = nullptr;
  };

  /// @brief Initializes Fwog's internal structures
  /// 
  /// Call at program start to initialize Fwog's internal state. Must be called after an OpenGL context has been acquired.
  /// @note Making any calls to Fwog before this function has been called will result in undefined behavior.
  void Initialize(const ContextInitializeInfo& contextInfo = {});

  /// @brief Destroys Fwog's internal structures
  ///
  /// Call at program exit or before Initialize is called again.
  void Terminate();

  /// @brief Invalidates assumptions Fwog has made about the OpenGL context state
  ///
  /// Call when OpenGL context state has been changed outside of Fwog (e.g., when using raw OpenGL or using an external
  /// library that calls OpenGL). This invalidates assumptions Fwog has made about the pipeline state for the purpose
  /// of state deduplication.
  void InvalidatePipelineState();

  /// @brief Query device properties
  /// @return A DeviceProperties struct containing information about the OpenGL context and device limits
  /// @note This call can replace most calls to glGet.
  const DeviceProperties& GetDeviceProperties();
} // namespace Fwog