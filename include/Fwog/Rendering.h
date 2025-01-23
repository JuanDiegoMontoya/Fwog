#pragma once
#include <Fwog/Config.h>
#include <Fwog/BasicTypes.h>
#include <Fwog/Texture.h>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <concepts>

namespace Fwog
{
  // clang-format off
  class Texture;
  class Sampler;
  class Buffer;
  struct GraphicsPipeline;
  struct ComputePipeline;

  // Minimal reference wrapper type. Didn't want to pull in <functional> just for this
  template <class T>
  requires std::is_object_v<T>
  class ReferenceWrapper
  {
  public:
    using type = T;

    template <class U>
    constexpr ReferenceWrapper(U&& val) noexcept
    {
      T& ref = static_cast<U&&>(val);
      ptr = std::addressof(ref);
    }

    constexpr operator T&() const noexcept
    {
      return *ptr;
    }

    [[nodiscard]] constexpr T& get() const noexcept
    {
      return *ptr;
    }

  private:
    T* ptr{};
  };

  /// @brief Describes a clear color value
  ///
  /// The clear color value can be up to four of float, uint32_t, or int32_t. Use of a type that does not match 
  /// the render target format may result in undefined behavior.
  struct ClearColorValue
  {
    ClearColorValue() = default;

    template<typename... Args>
    requires (sizeof...(Args) <= 4)
    ClearColorValue(const Args&... args)
      : data(std::array<std::common_type_t<std::remove_cvref_t<Args>...>, 4>{ args...})
    {
    }

    std::variant<std::array<float, 4>, std::array<uint32_t, 4>, std::array<int32_t, 4>> data;
  };

  /// @brief Tells Fwog what to do with a render target at the beginning of a pass
  enum class AttachmentLoadOp : uint32_t
  {
    /// @brief The previous contents of the image will be preserved
    LOAD,

    /// @brief The contents of the image will be cleared to a uniform value
    CLEAR,

    /// @brief The previous contents of the image need not be preserved (they may be discarded)
    DONT_CARE,
  };

  struct ClearDepthStencilValue
  {
    float depth{};
    int32_t stencil{};
  };

  struct RenderColorAttachment
  {
    ReferenceWrapper<const Texture> texture;
    AttachmentLoadOp loadOp = AttachmentLoadOp::LOAD;
    ClearColorValue clearValue;
  };
  
  struct RenderDepthStencilAttachment
  {
    ReferenceWrapper<const Texture> texture;
    AttachmentLoadOp loadOp = AttachmentLoadOp::LOAD;
    ClearDepthStencilValue clearValue;
  };
  
  struct Viewport
  {
    Rect2D drawRect = {};  // glViewport
    float minDepth = 0.0f; // glDepthRangef
    float maxDepth = 1.0f; // glDepthRangef
    ClipDepthRange depthRange = // glClipControl
#ifdef FWOG_DEFAULT_CLIP_DEPTH_RANGE_NEGATIVE_ONE_TO_ONE
      Fwog::ClipDepthRange::NEGATIVE_ONE_TO_ONE;
#else
      Fwog::ClipDepthRange::ZERO_TO_ONE;
#endif

    bool operator==(const Viewport&) const noexcept = default;
  };

  struct SwapchainRenderInfo
  {
    /// @brief An optional name to demarcate the pass in a graphics debugger
    std::string_view name;
    Viewport viewport = {};
    AttachmentLoadOp colorLoadOp = AttachmentLoadOp::LOAD;
    ClearColorValue clearColorValue;
    AttachmentLoadOp depthLoadOp = AttachmentLoadOp::LOAD;
    float clearDepthValue = 0.0f;
    AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::LOAD;
    int32_t clearStencilValue = 0;
    
    /// @brief If true, the linear->nonlinear sRGB OETF will be applied to pixels when rendering to the swapchain
    ///
    /// This facility is provided because OpenGL does not expose the swapchain as an image we can interact with 
    /// in the usual manner.
    bool enableSrgb = true;
  };

  // Describes the render targets that may be used in a draw
  struct RenderInfo
  {
    /// @brief An optional name to demarcate the pass in a graphics debugger
    std::string_view name;

    /// @brief An optional viewport
    /// 
    /// If empty, the viewport size will be the minimum the render targets' size and the offset will be 0.
    std::optional<Viewport> viewport = std::nullopt;
    std::span<const RenderColorAttachment> colorAttachments;
    std::optional<RenderDepthStencilAttachment> depthAttachment = std::nullopt;
    std::optional<RenderDepthStencilAttachment> stencilAttachment = std::nullopt;
  };

  // Describes the framebuffer state when rendering with no attachments (e.g., for algorithms that output to images or buffers).
  // Consult the documentation for glFramebufferParameteri for more info.
  struct RenderNoAttachmentsInfo
  {
    /// @brief An optional name to demarcate the pass in a graphics debugger
    std::string_view name;
    Viewport viewport{};
    Fwog::Extent3D framebufferSize{}; // If depth > 0, framebuffer is layered
    Fwog::SampleCount framebufferSamples{};
  };

  namespace detail
  {
    void BeginSwapchainRendering(const SwapchainRenderInfo& renderInfo);

    void BeginRendering(const RenderInfo& renderInfo);

    void BeginRenderingNoAttachments(const RenderNoAttachmentsInfo& info);

    void EndRendering();

    void BeginCompute(std::string_view name);

    void EndCompute();
  }
  
  /// @brief Renders to the swapchain
  /// @param renderInfo Rendering parameters
  /// @param func A callback that invokes rendering commands
  /// 
  /// The swapchain can be thought of as "the window". This function is provided because OpenGL nor 
  /// windowing libraries provide a simple mechanism to access the swapchain as a set of images without 
  /// interop with an explicit API like Vulkan or D3D12.
  void RenderToSwapchain(const SwapchainRenderInfo& renderInfo, const std::function<void()>& func);
  
  /// @brief Renders to a set of textures
  /// @param renderInfo Rendering parameters
  /// @param func A callback that invokes rendering commands
  void Render(const RenderInfo& renderInfo, const std::function<void()>& func);

  /// @brief Renders to a virtual texture
  /// @param renderInfo Rendering parameters
  /// @param func A callback that invokes rendering commands
  void RenderNoAttachments(const RenderNoAttachmentsInfo& renderInfo, const std::function<void()>& func);

  /// @brief Begins a compute scope
  /// @param func A callback that invokes dispatch commands
  void Compute(std::string_view name, const std::function<void()>& func);

  /// @brief Blits a texture to another texture. Supports minification and magnification
  void BlitTexture(const Texture& source,
                   const Texture& target,
                   Offset3D sourceOffset,
                   Offset3D targetOffset,
                   Extent3D sourceExtent,
                   Extent3D targetExtent,
                   Filter filter,
                   AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);

  /// @brief Blits a texture to the swapchain. Supports minification and magnification
  void BlitTextureToSwapchain(const Texture& source,
                              Offset3D sourceOffset,
                              Offset3D targetOffset,
                              Extent3D sourceExtent,
                              Extent3D targetExtent,
                              Filter filter,
                              AspectMask aspect = AspectMaskBit::COLOR_BUFFER_BIT);
  
  struct CopyTextureInfo
  {
    const Texture& source;
    Texture& target;
    uint32_t sourceLevel = 0;
    uint32_t targetLevel = 0;
    Offset3D sourceOffset = {};
    Offset3D targetOffset = {};
    Extent3D extent = {};
  };

  /// @brief Copies data between textures
  ///
  /// No format conversion is applied, as in memcpy.
  void CopyTexture(const CopyTextureInfo& copy);
  
  /// @brief Defines a barrier ordering memory transactions
  /// @param accessBits The barriers to insert
  /// 
  /// This call is used to ensure that incoherent writes (SSBO writes and image stores) from a shader
  /// are reflected in subsequent accesses.
  void MemoryBarrier(MemoryBarrierBits accessBits); // glMemoryBarrier

  /// @brief Allows subsequent draw commands to read the result of texels written in a previous draw operation
  ///
  /// See the ARB_texture_barrier spec for potential uses.
  void TextureBarrier(); // glTextureBarrier

  /// @brief Parameters for CopyBuffer()
  struct CopyBufferInfo
  {
    const Buffer& source;
    Buffer& target;
    uint64_t sourceOffset = 0;
    uint64_t targetOffset = 0;
    
    /// @brief The amount of data to copy, in bytes. If size is WHOLE_BUFFER, the size of the source buffer is used.
    uint64_t size = WHOLE_BUFFER;
  };

  /// @brief Copies data between buffers
  void CopyBuffer(const CopyBufferInfo& copy);

  /// @brief Parameters for CopyTextureToBuffer
  struct CopyTextureToBufferInfo
  {
    const Texture& sourceTexture;
    Buffer& targetBuffer;
    uint32_t level = 0;
    Offset3D sourceOffset = {};
    uint64_t targetOffset = {};
    Extent3D extent = {};
    UploadFormat format = UploadFormat::INFER_FORMAT;
    UploadType type = UploadType::INFER_TYPE;
    
    /// @brief Specifies, in texels, the size of rows in the buffer (for 2D and 3D images). If zero, it is assumed to be tightly packed according to \p extent
    uint32_t bufferRowLength = 0;
    
    /// @brief Specifies, in texels, the number of rows in the buffer (for 3D images. If zero, it is assumed to be tightly packed according to \p extent
    uint32_t bufferImageHeight = 0;
  };
  
  /// @brief Copies texture data into a buffer
  void CopyTextureToBuffer(const CopyTextureToBufferInfo& copy);
  
  struct CopyBufferToTextureInfo
  {
    const Buffer& sourceBuffer;
    Texture& targetTexture;
    uint32_t level = 0;
    uint64_t sourceOffset = {};
    Offset3D targetOffset = {};
    Extent3D extent = {};

    /// @brief The arrangement of components of texels in the source buffer. DEPTH_STENCIL is not allowed here
    UploadFormat format = UploadFormat::INFER_FORMAT;

    /// @brief The data type of the texel data
    UploadType type = UploadType::INFER_TYPE;

    /// @brief Specifies, in texels, the size of rows in the buffer (for 2D and 3D images). If zero, it is assumed to be tightly packed according to \p extent
    uint32_t bufferRowLength = 0;
    
    /// @brief Specifies, in texels, the number of rows in the buffer (for 3D images. If zero, it is assumed to be tightly packed according to \p extent
    uint32_t bufferImageHeight = 0;
  };

  /// @brief Copies buffer data into a texture
  void CopyBufferToTexture(const CopyBufferToTextureInfo& copy);

  /// @brief Functions that set pipeline state, binds resources, or issues draws or dispatches
  ///
  /// These functions are analogous to Vulkan vkCmd* calls, which can only be made inside of an active command buffer.
  /// 
  /// @note Calling functions in this namespace outside of a rendering or compute scope will result in undefined behavior
  namespace Cmd
  {
    /// @brief Binds a graphics pipeline to be used for future draw operations
    /// @param pipeline The pipeline to bind
    /// 
    /// Valid in rendering scopes.
    void BindGraphicsPipeline(const GraphicsPipeline& pipeline);

    /// @brief Binds a compute pipeline to be used for future dispatch operations
    /// @param pipeline The pipeline to bind
    /// 
    /// Valid in compute scopes.
    void BindComputePipeline(const ComputePipeline& pipeline);

    /// @brief Dynamically sets the viewport
    /// @param viewport The new viewport
    /// 
    /// Similar to glViewport. Valid in rendering scopes.
    void SetViewport(const Viewport& viewport);

    /// @brief Dynamically sets the scissor rect
    /// @param scissor The new scissor rect
    /// 
    /// Similar to glScissor. Valid in rendering scopes.
    void SetScissor(const Rect2D& scissor);

    /// @brief Equivalent to glDrawArraysInstancedBaseInstance or vkCmdDraw
    /// @param vertexCount The number of vertices to draw
    /// @param instanceCount The number of instances to draw
    /// @param firstVertex The index of the first vertex to draw
    /// @param firstInstance The instance ID of the first instance to draw
    /// 
    /// Valid in rendering scopes.
    void Draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance);

    /// @brief Equivalent to glDrawElementsInstancedBaseVertexBaseInstance or vkCmdDrawIndexed
    /// @param indexCount The number of vertices to draw
    /// @param instanceCount The number of instances to draw
    /// @param firstIndex The base index within the index buffer
    /// @param vertexOffset The value added to the vertex index before indexing into the vertex buffer
    /// @param firstInstance The instance ID of the first instance to draw
    /// 
    /// Valid in rendering scopes.
    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance);

    /// @brief Equivalent to glMultiDrawArraysIndirect or vkCmdDrawDrawIndirect
    /// @param commandBuffer The buffer containing draw parameters
    /// @param commandBufferOffset The byte offset into commandBuffer where parameters begin
    /// @param drawCount The number of draws to execute
    /// @param stride The byte stride between successive sets of draw parameters
    /// 
    /// Valid in rendering scopes.
    void DrawIndirect(const Buffer& commandBuffer,
                      uint64_t commandBufferOffset,
                      uint32_t drawCount,
                      uint32_t stride);

    /// @brief Equivalent to glMultiDrawArraysIndirectCount or vkCmdDrawIndirectCount
    /// @param commandBuffer The buffer containing draw parameters
    /// @param commandBufferOffset The byte offset into commandBuffer where parameters begin
    /// @param countBuffer The buffer containing the draw count
    /// @param countBufferOffset The byte offset into countBuffer where the draw count begins
    /// @param maxDrawCount The maximum number of draws that will be executed
    /// @param stride The byte stride between successive sets of draw parameters
    /// 
    /// Valid in rendering scopes.
    void DrawIndirectCount(const Buffer& commandBuffer,
                           uint64_t commandBufferOffset,
                           const Buffer& countBuffer,
                           uint64_t countBufferOffset,
                           uint32_t maxDrawCount,
                           uint32_t stride);

    /// @brief Equivalent to glMultiDrawElementsIndirect or vkCmdDrawIndexedIndirect
    /// @param commandBuffer The buffer containing draw parameters
    /// @param commandBufferOffset The byte offset into commandBuffer where parameters begin
    /// @param drawCount The number of draws to execute
    /// @param stride The byte stride between successive sets of draw parameters
    /// 
    /// Valid in rendering scopes.
    void DrawIndexedIndirect(const Buffer& commandBuffer,
                             uint64_t commandBufferOffset,
                             uint32_t drawCount,
                             uint32_t stride);

    /// @brief Equivalent to glMultiDrawElementsIndirectCount or vkCmdDrawIndexedIndirectCount
    /// @param commandBuffer The buffer containing draw parameters
    /// @param commandBufferOffset The byte offset into commandBuffer where parameters begin
    /// @param countBuffer The buffer containing the draw count
    /// @param countBufferOffset The byte offset into countBuffer where the draw count begins
    /// @param maxDrawCount The maximum number of draws that will be executed
    /// @param stride The byte stride between successive sets of draw parameters
    /// 
    /// Valid in rendering scopes.
    void DrawIndexedIndirectCount(const Buffer& commandBuffer,
                                  uint64_t commandBufferOffset,
                                  const Buffer& countBuffer,
                                  uint64_t countBufferOffset,
                                  uint32_t maxDrawCount,
                                  uint32_t stride);

    /// @brief Binds a buffer to a vertex buffer binding point
    ///
    /// Similar to glVertexArrayVertexBuffer. Valid in rendering scopes.
    void BindVertexBuffer(uint32_t bindingIndex, const Buffer& buffer, uint64_t offset, uint64_t stride);
    
    /// @brief Binds an index buffer
    ///
    /// Similar to glVertexArrayElementBuffer. Valid in rendering scopes.
    void BindIndexBuffer(const Buffer& buffer, IndexType indexType);
    
    /// @brief Binds a range within a buffer as a uniform buffer
    ///
    /// Similar to glBindBufferRange(GL_UNIFORM_BUFFER, ...)
    void BindUniformBuffer(uint32_t index, const Buffer& buffer, uint64_t offset = 0, uint64_t size = WHOLE_BUFFER);

    /// @brief Binds a range within a buffer as a uniform buffer
    /// @param block The name of the uniform block whose index to bind to
    /// @note Must be called after a pipeline is bound in order to get reflected program info
    void BindUniformBuffer(std::string_view block, const Buffer& buffer, uint64_t offset = 0, uint64_t size = WHOLE_BUFFER);
    
    /// @brief Binds a range within a buffer as a storage buffer
    ///
    /// Similar to glBindBufferRange(GL_SHADER_STORAGE_BUFFER, ...)
    void BindStorageBuffer(uint32_t index, const Buffer& buffer, uint64_t offset = 0, uint64_t size = WHOLE_BUFFER);
    
    /// @brief Binds a range within a buffer as a storage buffer
    /// @param block The name of the storage block whose index to bind to
    /// @note Must be called after a pipeline is bound in order to get reflected program info
    void BindStorageBuffer(std::string_view block, const Buffer& buffer, uint64_t offset = 0, uint64_t size = WHOLE_BUFFER);

    /// @brief Binds a texture and a sampler to a texture unit
    ///
    /// Similar to glBindTextureUnit + glBindSampler
    void BindSampledImage(uint32_t index, const Texture& texture, const Sampler& sampler);
    
    /// @brief Binds a texture and a sampler to a texture unit
    /// @param uniform The name of the uniform whose index to bind to
    /// @note Must be called after a pipeline is bound in order to get reflected program info
    void BindSampledImage(std::string_view uniform, const Texture& texture, const Sampler& sampler);

    /// @brief Binds a texture to an image unit
    ///
    /// Similar to glBindImageTexture{s}
    void BindImage(uint32_t index, const Texture& texture, uint32_t level);
    
    /// @brief Binds a texture to an image unit
    /// @param uniform The name of the uniform whose index to bind to
    /// @note Must be called after a pipeline is bound in order to get reflected program info
    void BindImage(std::string_view uniform, const Texture& texture, uint32_t level);

    /// @brief Invokes a compute shader
    /// @param groupCountX The number of local workgroups to dispatch in the X dimension
    /// @param groupCountY The number of local workgroups to dispatch in the Y dimension
    /// @param groupCountZ The number of local workgroups to dispatch in the Z dimension
    /// 
    /// Valid in compute scopes.
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

    /// @brief Invokes a compute shader
    /// @param groupCount The number of local workgroups to dispatch
    /// 
    /// Valid in compute scopes.
    void Dispatch(Extent3D groupCount);

    /// @brief Invokes a compute shader a specified number of times
    /// @param invocationCountX The minimum number of invocations in the X dimension
    /// @param invocationCountY The minimum number of invocations in the Y dimension
    /// @param invocationCountZ The minimum number of invocations in the Z dimension
    /// 
    /// Automatically computes the number of workgroups to invoke based on the formula 
    /// groupCount = (invocationCount + workgroupSize - 1) / workgroupSize.
    /// Valid in compute scopes.
    void DispatchInvocations(uint32_t invocationCountX, uint32_t invocationCountY, uint32_t invocationCountZ);
    
    /// @brief Invokes a compute shader a specified number of times
    /// @param invocationCount The minimum number of invocations
    /// 
    /// Automatically computes the number of workgroups to invoke based on the formula 
    /// groupCount = (invocationCount + workgroupSize - 1) / workgroupSize.
    /// Valid in compute scopes.
    void DispatchInvocations(Extent3D invocationCount);

    /// @brief Invokes a compute shader with at least as many threads as there are pixels in the image
    /// @param texture The texture from which to infer the dispatch size
    /// @param lod The level of detail of the tetxure from which to infer the dispatch size
    ///
    /// Automatically computes the number of workgroups to invoke based on the formula
    /// groupCount = (invocationCount + workgroupSize - 1) / workgroupSize.
    /// For 3D images, the depth is used for the Z component of invocationCount. Otherwise,
    /// the number of array layers will be used.
    /// For cube textures, the Z component of invocationCount will be equal to 6 times
    /// the number of array layers.
    /// Valid in compute scopes.
    void DispatchInvocations(const Texture& texture, uint32_t lod = 0);

    /// @brief Invokes a compute shader with the group count provided by a buffer
    /// @param commandBuffer The buffer containing dispatch parameters
    /// @param commandBufferOffset The byte offset into commandBuffer where the parameters begin
    /// 
    /// Valid in compute scopes.
    void DispatchIndirect(const Buffer& commandBuffer, uint64_t commandBufferOffset);

    // clang-format on
  } // namespace Cmd
} // namespace Fwog
