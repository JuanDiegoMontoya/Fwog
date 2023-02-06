#include <Fwog/Context.h>
#include <Fwog/detail/ContextState.h>

namespace Fwog
{
  namespace detail
  {
    void ZeroResourceBindings()
    {
      auto& limits = Fwog::detail::context->properties.limits;
      for (int i = 0; i < limits.maxImageUnits; i++)
      {
        glBindImageTexture(i, 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
      }

      for (int i = 0; i < limits.maxCombinedShaderStorageBlocks; i++)
      {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, 0, 0, 0);
      }

      for (int i = 0; i < limits.maxCombinedUniformBlocks; i++)
      {
        glBindBufferRange(GL_UNIFORM_BUFFER, i, 0, 0, 0);
      }

      for (int i = 0; i < limits.maxCombinedTextureImageUnits; i++)
      {
        glBindTextureUnit(i, 0);
        glBindSampler(i, 0);
      }
    }
  }
  static void QueryGlDeviceProperties(Fwog::DeviceProperties& properties)
  {
    properties.vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    properties.renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    properties.version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    properties.shadingLanguageVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    glGetIntegerv(GL_MAJOR_VERSION, &properties.glVersionMajor);
    glGetIntegerv(GL_MINOR_VERSION, &properties.glVersionMinor);

    // Populate limits
    auto& limits = properties.limits;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.maxTextureSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &limits.maxTextureSize3D);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limits.maxTextureSizeCube);

    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &limits.maxSamplerLodBias);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &limits.maxSamplerAnisotropy);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &limits.maxArrayTextureLayers);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, limits.maxViewportDims);
    glGetIntegerv(GL_SUBPIXEL_BITS, &limits.subpixelBits);

    glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &limits.maxFramebufferWidth);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &limits.maxFramebufferHeight);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_LAYERS, &limits.maxFramebufferLayers);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &limits.maxColorAttachments);

    glGetFloatv(GL_MIN_FRAGMENT_INTERPOLATION_OFFSET, &limits.interpolationOffsetRange[0]);
    glGetFloatv(GL_MAX_FRAGMENT_INTERPOLATION_OFFSET, &limits.interpolationOffsetRange[1]);
    glGetFloatv(GL_POINT_SIZE_GRANULARITY, &limits.pointSizeGranularity);
    glGetFloatv(GL_POINT_SIZE_RANGE, limits.pointSizeRange);
    glGetFloatv(GL_LINE_WIDTH_RANGE, limits.lineWidthRange);

    glGetIntegerv(GL_MAX_ELEMENT_INDEX, &limits.maxElementIndex);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limits.maxVertexAttribs);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &limits.maxVertexAttribBindings);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_STRIDE, &limits.maxVertexAttribStride);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, &limits.maxVertexAttribRelativeOffset);
    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &limits.maxVertexOutputComponents);
    glGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS, &limits.maxFragmentInputComponents);
    glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &limits.texelOffsetRange[0]);
    glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &limits.texelOffsetRange[1]);
    glGetIntegerv(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET, &limits.textureGatherOffsetRange[0]);
    glGetIntegerv(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET, &limits.textureGatherOffsetRange[1]);

    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &limits.maxUniformBufferBindings);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &limits.maxUniformBlockSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &limits.uniformBufferOffsetAlignment);
    glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &limits.maxCombinedUniformBlocks);

    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &limits.maxShaderStorageBufferBindings);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &limits.maxShaderStorageBlockSize);
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &limits.shaderStorageBufferOffsetAlignment);
    glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &limits.maxCombinedShaderStorageBlocks);

    glGetIntegerv(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES, &limits.maxCombinedShaderOutputResources);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limits.maxCombinedTextureImageUnits);

    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &limits.maxComputeSharedMemorySize);
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &limits.maxComputeWorkGroupInvocations);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &limits.maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &limits.maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &limits.maxComputeWorkGroupCount[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &limits.maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &limits.maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &limits.maxComputeWorkGroupSize[2]);

    glGetIntegerv(GL_MAX_IMAGE_UNITS, &limits.maxImageUnits);
    glGetIntegerv(GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS, &limits.maxFragmentCombinedOutputResources);
    glGetIntegerv(GL_MAX_COMBINED_IMAGE_UNIFORMS, &limits.maxCombinedImageUniforms);
    glGetIntegerv(GL_MAX_SERVER_WAIT_TIMEOUT, &limits.maxServerWaitTimeout);

    // Populate features
    auto& features = properties.features;

    GLint numExtensions{};
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; i++)
    {
      std::string_view extensionString = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
      // printf("%s\n", extensionString.data());
      if (extensionString == "GL_ARB_bindless_texture")
      {
        features.bindlessTextures = true;
      }
    }
  }

  void Initialize()
  {
    FWOG_ASSERT(Fwog::detail::context == nullptr && "Fwog has already been initialized");
    Fwog::detail::context = new Fwog::detail::ContextState;
    QueryGlDeviceProperties(Fwog::detail::context->properties);
  }

  void Terminate()
  {
    FWOG_ASSERT(Fwog::detail::context && "Fwog has already been terminated");
    delete Fwog::detail::context;
    Fwog::detail::context = nullptr;
  }

  void InvalidatePipelineState()
  {
    auto* context = Fwog::detail::context;

    FWOG_ASSERT(!context->isComputeActive && !context->isRendering);

#ifdef FWOG_DEBUG
    detail::ZeroResourceBindings();
#endif

    for (int i = 0; i < detail::MAX_COLOR_ATTACHMENTS; i++)
    {
      ColorComponentFlags& flags = context->lastColorMask[i];
      flags = ColorComponentFlag::RGBA_BITS;
      glColorMaski(i, true, true, true, true);
    }

    context->lastDepthMask = false;
    glDepthMask(false);

    context->lastStencilMask[0] = 0;
    context->lastStencilMask[1] = 0;
    glStencilMask(false);
    
    context->currentFbo = 0;
    context->currentVao = 0;
    context->lastGraphicsPipeline.reset();
    context->initViewport = true;
    context->lastScissor = {};
    
    glEnable(GL_FRAMEBUFFER_SRGB);
  }

  const DeviceProperties& GetDeviceProperties()
  {
    return Fwog::detail::context->properties;
  }
} // namespace Fwog