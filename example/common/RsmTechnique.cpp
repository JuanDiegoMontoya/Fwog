#include "RsmTechnique.h"
#include "Application.h"

#include <Fwog/DebugMarker.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>

#include <imgui.h>

#include <stb_image.h>
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>

#include <memory>
#include <utility>

static glm::uint pcg_hash(glm::uint seed)
{
  glm::uint state = seed * 747796405u + 2891336453u;
  glm::uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// Used to advance the PCG state.
static glm::uint rand_pcg(glm::uint& rng_state)
{
  glm::uint state = rng_state;
  rng_state = rng_state * 747796405u + 2891336453u;
  glm::uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// Advances the prng state and returns the corresponding random float.
static float rng(glm::uint& state)
{
  glm::uint x = rand_pcg(state);
  state = x;
  return float(x) * glm::uintBitsToFloat(0x2f800004u);
}

static std::string LoadFileWithInclude(std::string_view path)
{
  char error[256] = {};
  char* included = stb_include_string(Application::LoadFile(path).data(), nullptr, "shaders/rsm", "", error);
  std::string includedStr = included;
  free(included);
  return includedStr;
}

static Fwog::ComputePipeline CreateRsmIndirectPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/rsm/Indirect.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateRsmIndirectFilteredPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER,
                         Application::LoadFile("shaders/rsm/IndirectDitheredFiltered.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateRsmReprojectPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Reproject.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateBilateral5x5Pipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Bilateral5x5.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateModulatePipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Modulate.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateModulateUpscalePipeline()
{
  auto cs =
    Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/ModulateUpscale.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateBlitPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/BlitTexture.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

namespace RSM
{
  RsmTechnique::RsmTechnique(uint32_t width_, uint32_t height_)
    : seedX(pcg_hash(17)),
      seedY(pcg_hash(seedX)),
      rsmUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      cameraUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      reprojectionUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      filterUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      rsmIndirectPipeline(CreateRsmIndirectPipeline()),
      rsmIndirectFilteredPipeline(CreateRsmIndirectFilteredPipeline()),
      rsmReprojectPipeline(CreateRsmReprojectPipeline()),
      bilateral5x5Pipeline(CreateBilateral5x5Pipeline()),
      modulatePipeline(CreateModulatePipeline()),
      modulateUpscalePipeline(CreateModulateUpscalePipeline()),
      blitPipeline(CreateBlitPipeline())
  {
    SetResolution(width_, height_);

    // load blue noise texture
    int x = 0;
    int y = 0;
    auto noise = std::unique_ptr<stbi_uc, decltype([](stbi_uc* p) { stbi_image_free(p); })>(
      stbi_load("textures/bluenoise256.png", &x, &y, nullptr, 4));

    assert(noise);
    noiseTex = Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
    noiseTex->UpdateImage({
      .level = 0,
      .offset = {},
      .extent = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .pixels = noise.get(),
    });
  }

  void RsmTechnique::SetResolution(uint32_t newWidth, uint32_t newHeight)
  {
    width = newWidth;
    height = newHeight;
    internalWidth = width / inverseResolutionScale;
    internalHeight = height / inverseResolutionScale;
    indirectUnfilteredTex = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R16G16B16A16_FLOAT);
    indirectUnfilteredTexPrev = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R16G16B16A16_FLOAT);
    indirectFilteredTex = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R16G16B16A16_FLOAT);
    indirectFilteredTexPingPong = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R16G16B16A16_FLOAT);
    historyLengthTex = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R8_UINT);
    illuminationUpscaled = Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT);
    rsmFluxSmall = Fwog::CreateTexture2D({(uint32_t)smallRsmSize, (uint32_t)smallRsmSize}, Fwog::Format::R11G11B10_FLOAT);
    rsmNormalSmall = Fwog::CreateTexture2D({(uint32_t)smallRsmSize, (uint32_t)smallRsmSize}, Fwog::Format::R8G8B8A8_SNORM);
    rsmDepthSmall = Fwog::CreateTexture2D({(uint32_t)smallRsmSize, (uint32_t)smallRsmSize}, Fwog::Format::R32_FLOAT);

    if (inverseResolutionScale > 1)
    {
      gNormalSmall = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R8G8B8A8_SNORM);
      gNormalPrevSmall = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R8G8B8A8_SNORM);
      gDepthSmall = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R32_FLOAT);
      gDepthPrevSmall = Fwog::CreateTexture2D({internalWidth, internalHeight}, Fwog::Format::R32_FLOAT);
    }
    else
    {
      gNormalSmall.reset();
      gNormalPrevSmall.reset();
      gDepthSmall.reset();
      gDepthPrevSmall.reset();
    }

    historyLengthTex->ClearImage({
      .extent = historyLengthTex->Extent(),
      .format = Fwog::UploadFormat::R_INTEGER,
      .type = Fwog::UploadType::UBYTE,
      .data = nullptr,
    });

    indirectUnfilteredTex->ClearImage({
      .extent = indirectUnfilteredTex->Extent(),
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .data = nullptr,
    });
  }

  void RsmTechnique::ComputeIndirectLighting(const glm::mat4& lightViewProj,
                                             const CameraUniforms& cameraUniforms,
                                             const Fwog::Texture& gAlbedo,
                                             const Fwog::Texture& gNormal,
                                             const Fwog::Texture& gDepth,
                                             const Fwog::Texture& rsmFlux,
                                             const Fwog::Texture& rsmNormal,
                                             const Fwog::Texture& rsmDepth,
                                             const Fwog::Texture& gDepthPrev,
                                             const Fwog::Texture& gNormalPrev,
                                             const Fwog::Texture& gMotion)
  {
    Fwog::SamplerState ss;
    ss.minFilter = Fwog::Filter::NEAREST;
    ss.magFilter = Fwog::Filter::NEAREST;
    ss.addressModeU = Fwog::AddressMode::REPEAT;
    ss.addressModeV = Fwog::AddressMode::REPEAT;
    auto nearestSampler = Fwog::Sampler(ss);

    ss.borderColor = Fwog::BorderColor::FLOAT_TRANSPARENT_BLACK;
    ss.addressModeU = Fwog::AddressMode::CLAMP_TO_BORDER;
    ss.addressModeV = Fwog::AddressMode::CLAMP_TO_BORDER;
    auto nearestSamplerClamped = Fwog::Sampler(ss);

    ss.minFilter = Fwog::Filter::LINEAR;
    ss.magFilter = Fwog::Filter::LINEAR;
    auto linearSampler = Fwog::Sampler(ss);

    rsmUniforms = {
      .sunViewProj = lightViewProj,
      .invSunViewProj = glm::inverse(lightViewProj),
      .rMax = rMax,
      .samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples),
    };

    if (seedEachFrame)
    {
      rsmUniforms.random = glm::vec2(rng(seedX), rng(seedY));
    }
    else
    {
      rsmUniforms.random = glm::vec2(0);
    }

    if (rsmFiltered)
    {
      rsmUniforms.targetDim = {internalWidth, internalHeight};
    }
    else
    {
      rsmUniforms.targetDim = {illuminationUpscaled->Extent().width, illuminationUpscaled->Extent().height};
    }

    rsmUniformBuffer.UpdateData(rsmUniforms);

    cameraUniformBuffer.UpdateData(cameraUniforms);

    Fwog::Compute(
      "Indirect Illumination",
      [&]
      {
        Fwog::Cmd::BindSampledImage(0, *indirectUnfilteredTex, nearestSampler);
        Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
        Fwog::Cmd::BindSampledImage(2, gNormalSmall ? *gNormalSmall : gNormal, nearestSampler);
        Fwog::Cmd::BindSampledImage(3, gDepthSmall ? *gDepthSmall : gDepth, nearestSampler);
        Fwog::Cmd::BindSampledImage(4, *rsmFluxSmall, nearestSamplerClamped);
        Fwog::Cmd::BindSampledImage(5, *rsmNormalSmall, nearestSampler);
        Fwog::Cmd::BindSampledImage(6, *rsmDepthSmall, nearestSampler);
        Fwog::Cmd::BindUniformBuffer(0, cameraUniformBuffer);
        Fwog::Cmd::BindUniformBuffer(1, rsmUniformBuffer);

        if (rsmFiltered)
        {
          const auto workSize = Fwog::Extent3D{(uint32_t)rsmUniforms.targetDim.x, (uint32_t)rsmUniforms.targetDim.y, 1};

          if (inverseResolutionScale > 1)
          {
            Fwog::ScopedDebugMarker marker2("Downsample G-buffer");

            Fwog::Cmd::BindComputePipeline(blitPipeline);
            Fwog::Cmd::BindSampledImage(0, gNormal, nearestSampler);
            Fwog::Cmd::BindImage(0, *gNormalSmall, 0);
            Fwog::Cmd::DispatchInvocations(workSize);

            Fwog::Cmd::BindSampledImage(0, gNormalPrev, nearestSampler);
            Fwog::Cmd::BindImage(0, *gNormalPrevSmall, 0);
            Fwog::Cmd::DispatchInvocations(workSize);

            Fwog::Cmd::BindSampledImage(0, gDepth, nearestSampler);
            Fwog::Cmd::BindImage(0, *gDepthSmall, 0);
            Fwog::Cmd::DispatchInvocations(workSize);

            Fwog::Cmd::BindSampledImage(0, gDepthPrev, nearestSampler);
            Fwog::Cmd::BindImage(0, *gDepthPrevSmall, 0);
            Fwog::Cmd::DispatchInvocations(workSize);
          }

          {
            Fwog::ScopedDebugMarker marker2("Downsample RSM");

            Fwog::Cmd::BindComputePipeline(blitPipeline);

            Fwog::Cmd::BindSampledImage(0, rsmFlux, nearestSampler);
            Fwog::Cmd::BindImage(0, *rsmFluxSmall, 0);
            Fwog::Cmd::DispatchInvocations(rsmFluxSmall->Extent());

            Fwog::Cmd::BindSampledImage(0, rsmNormal, nearestSampler);
            Fwog::Cmd::BindImage(0, *rsmNormalSmall, 0);
            Fwog::Cmd::DispatchInvocations(rsmNormalSmall->Extent());

            Fwog::Cmd::BindSampledImage(0, rsmDepth, nearestSampler);
            Fwog::Cmd::BindImage(0, *rsmDepthSmall, 0);
            Fwog::Cmd::DispatchInvocations(rsmDepthSmall->Extent());
          }

          rsmUniforms.currentPass = 0;

          // Evaluate indirect illumination (sample the reflective shadow map)
          {
            Fwog::ScopedDebugMarker marker2("Sample RSM");

            Fwog::Cmd::BindComputePipeline(rsmIndirectFilteredPipeline);
            Fwog::Cmd::BindSampledImage(7, *noiseTex, nearestSampler);
            rsmUniformBuffer.UpdateData(rsmUniforms);
            Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT | Fwog::MemoryBarrierBit::IMAGE_ACCESS_BIT);
            Fwog::Cmd::BindImage(0, *indirectUnfilteredTex, 0);
            Fwog::Cmd::DispatchInvocations(workSize);
          }

          // Temporally accumulate samples before filtering
          {
            Fwog::ScopedDebugMarker marker2("Temporal Accumulation");

            ReprojectionUniforms reprojectionUniforms = {
              .invViewProjCurrent = cameraUniforms.invViewProj,
              .viewProjPrevious = viewProjPrevious,
              .invViewProjPrevious = glm::inverse(viewProjPrevious),
              .proj = cameraUniforms.proj,
              .viewPos = cameraUniforms.cameraPos,
              .temporalWeightFactor = spatialFilterStep,
              .targetDim = {indirectUnfilteredTex->Extent().width, indirectUnfilteredTex->Extent().height},
              .alphaIlluminance = alphaIlluminance,
              .phiDepth = phiDepth,
              .phiNormal = phiNormal,
              .jitterOffset = cameraUniforms.jitterOffset,
              .lastFrameJitterOffset = cameraUniforms.lastFrameJitterOffset,
            };
            viewProjPrevious = cameraUniforms.viewProj;
            reprojectionUniformBuffer.UpdateData(reprojectionUniforms);
            Fwog::Cmd::BindComputePipeline(rsmReprojectPipeline);
            Fwog::Cmd::BindSampledImage(0, *indirectUnfilteredTex, nearestSampler);
            Fwog::Cmd::BindSampledImage(1, *indirectUnfilteredTexPrev, linearSampler);
            Fwog::Cmd::BindSampledImage(2, gDepthSmall ? *gDepthSmall : gDepth, nearestSampler);
            Fwog::Cmd::BindSampledImage(3, gDepthPrevSmall ? *gDepthPrevSmall : gDepthPrev, linearSampler);
            Fwog::Cmd::BindSampledImage(4, gNormalSmall ? *gNormalSmall : gNormal, nearestSampler);
            Fwog::Cmd::BindSampledImage(5, gNormalPrevSmall ? *gNormalPrevSmall : gNormalPrev, linearSampler);
            Fwog::Cmd::BindSampledImage(6, gMotion, linearSampler);
            Fwog::Cmd::BindImage(0, *indirectFilteredTex, 0);
            Fwog::Cmd::BindImage(1, *historyLengthTex, 0);
            Fwog::Cmd::BindUniformBuffer(0, reprojectionUniformBuffer);
            Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT | Fwog::MemoryBarrierBit::IMAGE_ACCESS_BIT);
            Fwog::Cmd::DispatchInvocations(workSize);
          }

          FilterUniforms filterUniforms = {
            .proj = cameraUniforms.proj,
            .invViewProj = cameraUniforms.invViewProj,
            .viewPos = cameraUniforms.cameraPos,
            .targetDim = {indirectUnfilteredTex->Extent().width, indirectFilteredTex->Extent().height},
            .phiNormal = phiNormal,
            .phiDepth = phiDepth,
          };

          {
            Fwog::ScopedDebugMarker marker2("Filter");

            Fwog::Cmd::BindComputePipeline(bilateral5x5Pipeline);
            Fwog::Cmd::BindSampledImage(1, gNormalSmall ? *gNormalSmall : gNormal, nearestSampler);
            Fwog::Cmd::BindSampledImage(2, gDepthSmall ? *gDepthSmall : gDepth, nearestSampler);
            Fwog::Cmd::BindSampledImage(3, *historyLengthTex, nearestSampler);
            Fwog::Cmd::BindUniformBuffer(0, filterUniformBuffer);

            if (useSeparableFilter)
            {
              filterUniforms.stepWidth = 1 * spatialFilterStep;

              filterUniforms.direction = {0, 1};
              filterUniformBuffer.UpdateData(filterUniforms);
              Fwog::Cmd::BindSampledImage(0, *indirectFilteredTex, nearestSampler);
              Fwog::Cmd::BindImage(0, *indirectUnfilteredTex, 0);
              Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
              Fwog::Cmd::DispatchInvocations(workSize);

              filterUniforms.direction = {1, 0};
              filterUniformBuffer.UpdateData(filterUniforms);
              Fwog::Cmd::BindSampledImage(0, *indirectUnfilteredTex, nearestSampler);
              Fwog::Cmd::BindImage(0, *indirectUnfilteredTexPrev, 0);
              Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
              Fwog::Cmd::DispatchInvocations(workSize);

              for (int i = 1; i < 5 - std::log2f(float(inverseResolutionScale)); i++)
              {
                filterUniforms.stepWidth = (1 << i) * spatialFilterStep;

                filterUniforms.direction = {0, 1};
                filterUniformBuffer.UpdateData(filterUniforms);
                Fwog::Cmd::BindSampledImage(0, i == 1 ? *indirectUnfilteredTexPrev : *indirectFilteredTex, nearestSampler);
                Fwog::Cmd::BindImage(0, *indirectUnfilteredTex, 0);
                Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
                Fwog::Cmd::DispatchInvocations(workSize);

                filterUniforms.direction = {1, 0};
                filterUniformBuffer.UpdateData(filterUniforms);
                Fwog::Cmd::BindSampledImage(0, *indirectUnfilteredTex, nearestSampler);
                Fwog::Cmd::BindImage(0, *indirectFilteredTex, 0);
                Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
                Fwog::Cmd::DispatchInvocations(workSize);
              }
            }
            else
            {
              filterUniforms.direction = {0, 0};

              for (int i = 0; i < 5 - std::log2f(float(inverseResolutionScale)); i++)
              {
                filterUniforms.stepWidth = (1 << i) * spatialFilterStep;
                filterUniformBuffer.UpdateData(filterUniforms);

                // The output of the first filter pass gets stored in the history
                const Fwog::Texture* in{};
                const Fwog::Texture* out{};
                if (i == 0)
                {
                  in = &indirectFilteredTex.value();
                  out = &indirectUnfilteredTexPrev.value();
                }
                else if (i == 1)
                {
                  in = &indirectUnfilteredTexPrev.value();
                  out = &indirectUnfilteredTex.value();
                }
                else if (i % 2 == 0)
                {
                  in = &indirectUnfilteredTex.value();
                  out = &indirectFilteredTex.value();
                }
                else
                {
                  in = &indirectFilteredTex.value();
                  out = &indirectUnfilteredTex.value();
                }

                Fwog::Cmd::BindSampledImage(0, *in, nearestSampler);
                Fwog::Cmd::BindImage(0, *out, 0);
                Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
                Fwog::Cmd::DispatchInvocations(workSize);
              }
            }
          }

          auto& illuminationOutTex = inverseResolutionScale == 1 ? indirectFilteredTexPingPong : illuminationUpscaled;
          if (!rsmFilteredSkipAlbedoModulation)
          {
            // No upscale required
            if (inverseResolutionScale == 1)
            {
              Fwog::ScopedDebugMarker marker2("Modulate Albedo");

              Fwog::Cmd::BindComputePipeline(modulatePipeline);
              Fwog::Cmd::BindSampledImage(0, *indirectFilteredTex, nearestSampler);
              Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
              Fwog::Cmd::BindImage(0, *illuminationOutTex, 0);
              Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
              Fwog::Cmd::DispatchInvocations(illuminationOutTex->Extent());
            }
            else // Use bilateral upscale
            {
              Fwog::ScopedDebugMarker marker2("Modulate Albedo (Upscale)");

              Fwog::Cmd::BindComputePipeline(modulateUpscalePipeline);

              if (!useSeparableFilter && (5 - int(std::log2f(float(inverseResolutionScale)))) % 2 == 0)
              {
                Fwog::Cmd::BindSampledImage(0, *indirectUnfilteredTex, nearestSampler);
              }
              else
              {
                Fwog::Cmd::BindSampledImage(0, *indirectFilteredTex, nearestSampler);
              }

              Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
              Fwog::Cmd::BindSampledImage(2, gNormal, nearestSampler);
              Fwog::Cmd::BindSampledImage(3, gDepth, nearestSampler);
              Fwog::Cmd::BindSampledImage(4, *gNormalSmall, nearestSampler);
              Fwog::Cmd::BindSampledImage(5, *gDepthSmall, nearestSampler);
              filterUniforms.targetDim = {illuminationOutTex->Extent().width, illuminationOutTex->Extent().height};
              filterUniformBuffer.UpdateData(filterUniforms);
              Fwog::Cmd::BindUniformBuffer(0, filterUniformBuffer);
              Fwog::Cmd::BindImage(0, *illuminationOutTex, 0);
              Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
              Fwog::Cmd::DispatchInvocations(illuminationOutTex->Extent());
            }
          }
          else
          {
            Fwog::BlitTexture(*indirectFilteredTex,
                              *illuminationOutTex,
                              {},
                              {},
                              indirectFilteredTex->Extent(),
                              illuminationOutTex->Extent(),
                              Fwog::Filter::NEAREST);
          }
        }
        else // Unfiltered RSM: the original paper
        {
          Fwog::Cmd::BindComputePipeline(rsmIndirectPipeline);
          Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
          Fwog::Cmd::BindSampledImage(2, gNormal, nearestSampler);
          Fwog::Cmd::BindSampledImage(3, gDepth, nearestSampler);
          Fwog::Cmd::BindSampledImage(4, rsmFlux, nearestSamplerClamped);
          Fwog::Cmd::BindSampledImage(5, rsmNormal, nearestSampler);
          Fwog::Cmd::BindSampledImage(6, rsmDepth, nearestSampler);
          Fwog::Cmd::BindSampledImage(0, *illuminationUpscaled, nearestSampler);
          Fwog::Cmd::BindImage(0, *illuminationUpscaled, 0);

          const auto workgroupSize = rsmIndirectFilteredPipeline.WorkgroupSize();
          const auto workSize =
            Fwog::Extent3D{(uint32_t)rsmUniforms.targetDim.x / 2, (uint32_t)rsmUniforms.targetDim.y / 2, 1};

          // Quarter resolution indirect illumination pass
          rsmUniforms.currentPass = 0;
          rsmUniformBuffer.UpdateData(rsmUniforms);
          Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::DispatchInvocations(workSize);

          // Reconstruction pass 1
          rsmUniforms.currentPass = 1;
          rsmUniformBuffer.UpdateData(rsmUniforms);
          Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::DispatchInvocations(workSize);

          // Reconstruction pass 2
          rsmUniforms.currentPass = 2;
          rsmUniformBuffer.UpdateData(rsmUniforms);
          Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::DispatchInvocations(workSize);

          // Reconstruction pass 3
          rsmUniforms.currentPass = 3;
          rsmUniformBuffer.UpdateData(rsmUniforms);
          Fwog::MemoryBarrier(Fwog::MemoryBarrierBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::DispatchInvocations(workSize);
        }
      });
  }

  Fwog::Texture& RsmTechnique::GetIndirectLighting()
  {
    if (rsmFiltered)
    {
      return inverseResolutionScale == 1 ? *indirectFilteredTexPingPong : *illuminationUpscaled;
    }

    return *illuminationUpscaled;
  }

  void RsmTechnique::DrawGui()
  {
    ImGui::Checkbox("Use Filtered RSM", &rsmFiltered);

    // This setting blows everything up, so it'll be hardcoded for now
    // if (ImGui::SliderInt("Inverse Res. Scale", &inverseResolutionScale, 1, 4))
    //{
    //  SetResolution(width, height);
    //}
    if (ImGui::SliderInt("Small RSM Size", &smallRsmSize, 64, 1024))
    {
      SetResolution(width, height);
    }
    ImGui::SliderFloat("rMax", &rMax, 0.02f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::PushButtonRepeat(true);

    if (!rsmFiltered)
    {
      ImGui::SliderInt("Samples##Uniltered", &rsmSamples, 1, 400);
      ImGui::SameLine();
      if (ImGui::Button(" - "))
        rsmSamples--;
      ImGui::SameLine();
      if (ImGui::Button(" + "))
        rsmSamples++;
      rsmSamples = rsmSamples <= 0 ? 1 : rsmSamples;
    }
    else
    {
      ImGui::SliderInt("Samples##Filtered", &rsmFilteredSamples, 1, 40);
      ImGui::SameLine();
      if (ImGui::Button(" - "))
        rsmFilteredSamples--;
      ImGui::SameLine();
      if (ImGui::Button(" + "))
        rsmFilteredSamples++;
      rsmFilteredSamples = rsmFilteredSamples <= 0 ? 1 : rsmFilteredSamples;

      float epsilon = 1e-2f;
      ImGui::SliderFloat("Alpha Illuminance", &alphaIlluminance, epsilon, 1);
      ImGui::SliderFloat("Phi Normal", &phiNormal, epsilon, 1);
      ImGui::SliderFloat("Phi Depth", &phiDepth, epsilon, 1);
      ImGui::SliderFloat("Spatial Filter Step", &spatialFilterStep, 0, 1);
      ImGui::Checkbox("Skip Albedo Modulation", &rsmFilteredSkipAlbedoModulation);
      ImGui::Checkbox("Seed Each Frame", &seedEachFrame);
      ImGui::Checkbox("Use Separable Filter", &useSeparableFilter);
    }

    rsmUniforms.samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples);

    ImGui::PopButtonRepeat();
  }
} // namespace RSM
