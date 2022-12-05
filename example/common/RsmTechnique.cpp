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
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Reproject2.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateBilateral3x3Pipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Bilateral3x3.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateBilateral5x5Pipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Bilateral5x5.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateVariancePipeline()
{
  auto cs =
    Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/ComputeVariance5x5.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateModulatePipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFileWithInclude("shaders/rsm/Modulate.comp.glsl"));
  return Fwog::ComputePipeline({.shader = &cs});
}

namespace RSM
{
  RsmTechnique::RsmTechnique(uint32_t width, uint32_t height)
    : seedX(pcg_hash(17)),
      seedY(pcg_hash(seedX)),
      rsmUniforms({
        .targetDim = {width, height},
        .rMax = rMax,
        .samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples),
      }),
      cameraUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      reprojectionUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      rsmUniformBuffer(rsmUniforms, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      filterUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      rsmIndirectPipeline(CreateRsmIndirectPipeline()),
      rsmIndirectFilteredPipeline(CreateRsmIndirectFilteredPipeline()),
      rsmReprojectPipeline(CreateRsmReprojectPipeline()),
      bilateral3x3Pipeline(CreateBilateral3x3Pipeline()),
      bilateral5x5Pipeline(CreateBilateral5x5Pipeline()),
      variancePipeline(CreateVariancePipeline()),
      modulatePipeline(CreateModulatePipeline()),
      indirectUnfilteredTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT)),
      indirectUnfilteredTexPrev(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT)),
      indirectFilteredTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT)),
      indirectFilteredTexPingPong(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT)),
      historyLengthTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R8_UINT)),
      momentsTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16_FLOAT)),
      momentsHistoryTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16_FLOAT)),
      varianceTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16_FLOAT))
  {
    // load blue noise texture
    int x = 0;
    int y = 0;
    auto noise = std::unique_ptr<stbi_uc, decltype([](stbi_uc* p) { stbi_image_free(p); })>(
      stbi_load("textures/bluenoise256.png", &x, &y, nullptr, 4));

    assert(noise);
    noiseTex = Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
    noiseTex->SubImage({
      .dimension = Fwog::UploadDimension::TWO,
      .level = 0,
      .offset = {},
      .size = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .pixels = noise.get(),
    });

    historyLengthTex.ClearImage({
      .size = historyLengthTex.Extent(),
      .format = Fwog::UploadFormat::R_INTEGER,
      .type = Fwog::UploadType::UBYTE,
      .data = nullptr,
    });

    indirectUnfilteredTex.ClearImage({
      .size = indirectUnfilteredTex.Extent(),
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .data = nullptr,
    });

    momentsTex.ClearImage({
      .size = indirectUnfilteredTex.Extent(),
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .data = nullptr,
    });

    momentsHistoryTex.ClearImage({
      .size = indirectUnfilteredTex.Extent(),
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
                                             const Fwog::Texture& gNormalPrev)
  {
    Fwog::SamplerState ss;
    ss.minFilter = Fwog::Filter::NEAREST;
    ss.magFilter = Fwog::Filter::NEAREST;
    ss.addressModeU = Fwog::AddressMode::REPEAT;
    ss.addressModeV = Fwog::AddressMode::REPEAT;
    auto nearestSampler = Fwog::Sampler(ss);

    ss.minFilter = Fwog::Filter::LINEAR;
    ss.magFilter = Fwog::Filter::LINEAR;
    auto linearSampler = Fwog::Sampler(ss);

    ss.borderColor = Fwog::BorderColor::FLOAT_TRANSPARENT_BLACK;
    ss.addressModeU = Fwog::AddressMode::CLAMP_TO_BORDER;
    ss.addressModeV = Fwog::AddressMode::CLAMP_TO_BORDER;
    auto nearestSamplerClamped = Fwog::Sampler(ss);

    rsmUniforms.sunViewProj = lightViewProj;
    rsmUniforms.invSunViewProj = glm::inverse(rsmUniforms.sunViewProj);
    if (seedEachFrame)
    {
      rsmUniforms.random = glm::vec2(rng(seedX), rng(seedY));
    }
    else
    {
      rsmUniforms.random = glm::vec2(0);
    }
    rsmUniformBuffer.SubData(rsmUniforms, 0);

    cameraUniformBuffer.SubDataTyped(cameraUniforms);

    // std::swap(indirectUnfilteredTex, indirectUnfilteredTexPrev);
    std::swap(momentsTex, momentsHistoryTex);

    Fwog::BeginCompute();
    {
      Fwog::ScopedDebugMarker marker("Indirect Illumination");

      Fwog::Cmd::BindSampledImage(0, indirectUnfilteredTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, gNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, gDepth, nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rsmFlux, nearestSamplerClamped);
      Fwog::Cmd::BindSampledImage(5, rsmNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(6, rsmDepth, nearestSampler);
      Fwog::Cmd::BindUniformBuffer(0, cameraUniformBuffer, 0, cameraUniformBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, rsmUniformBuffer, 0, rsmUniformBuffer.Size());

      if (rsmFiltered)
      {
        const int localSize = 8;
        const auto numGroups = (rsmUniforms.targetDim + localSize - 1) / localSize;

        uint32_t currentPass = 0;

        // Evaluate indirect illumination (sample the reflective shadow map)
        {
          Fwog::ScopedDebugMarker marker2("Sample RSM");
          Fwog::Cmd::BindComputePipeline(rsmIndirectFilteredPipeline);
          Fwog::Cmd::BindSampledImage(7, *noiseTex, nearestSampler);
          rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT |
                                   Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
          Fwog::Cmd::BindImage(0, indirectUnfilteredTex, 0);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
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
            .targetDim = {indirectUnfilteredTex.Extent().width, indirectUnfilteredTex.Extent().height},
            .alphaIlluminance = alphaIlluminance,
            .alphaMoments = alphaMoments,
            .phiDepth = phiDepth,
            .phiNormal = phiNormal,
          };
          viewProjPrevious = cameraUniforms.viewProj;
          reprojectionUniformBuffer.SubDataTyped(reprojectionUniforms);
          Fwog::Cmd::BindComputePipeline(rsmReprojectPipeline);
          Fwog::Cmd::BindSampledImage(0, indirectUnfilteredTex, nearestSampler);
          Fwog::Cmd::BindSampledImage(1, indirectUnfilteredTexPrev, linearSampler);
          Fwog::Cmd::BindSampledImage(2, gDepth, nearestSampler);
          Fwog::Cmd::BindSampledImage(3, gDepthPrev, linearSampler);
          Fwog::Cmd::BindSampledImage(4, gNormal, nearestSampler);
          Fwog::Cmd::BindSampledImage(5, gNormalPrev, linearSampler);
          Fwog::Cmd::BindSampledImage(6, momentsHistoryTex, linearSampler);
          Fwog::Cmd::BindImage(0, indirectFilteredTex, 0);
          Fwog::Cmd::BindImage(1, momentsTex, 0);
          Fwog::Cmd::BindImage(2, historyLengthTex, 0);
          Fwog::Cmd::BindUniformBuffer(0, reprojectionUniformBuffer, 0, reprojectionUniformBuffer.Size());
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT |
                                   Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
        }

        // Compute the spatial variance of the luminance
        {
          Fwog::ScopedDebugMarker marker2("Variance");
          Fwog::Cmd::BindComputePipeline(variancePipeline);
          Fwog::Cmd::BindSampledImage(0, indirectFilteredTex, nearestSampler);
          Fwog::Cmd::BindSampledImage(1, historyLengthTex, nearestSampler);
          Fwog::Cmd::BindImage(0, momentsTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
        }

        {
          Fwog::ScopedDebugMarker marker2("Filter");
          Fwog::Cmd::BindComputePipeline(bilateral5x5Pipeline);
          Fwog::Cmd::BindSampledImage(1, gNormal, nearestSampler);
          Fwog::Cmd::BindSampledImage(2, gDepth, nearestSampler);
          Fwog::Cmd::BindSampledImage(3, momentsTex, nearestSampler);
          Fwog::Cmd::BindSampledImage(4, historyLengthTex, nearestSampler);
          Fwog::Cmd::BindUniformBuffer(0, filterUniformBuffer, 0, filterUniformBuffer.Size());
          FilterUniforms filterUniforms = {
            .proj = cameraUniforms.proj,
            .invViewProj = cameraUniforms.invViewProj,
            .viewPos = cameraUniforms.cameraPos,
            .targetDim = {indirectUnfilteredTex.Extent().width, indirectFilteredTex.Extent().height},
            .phiLuminance = phiLuminance,
            .phiNormal = phiNormal,
            .phiDepth = phiDepth,
          };

          // The output of the first filter pass gets stored in the history
          filterUniforms.stepWidth = 1 * spatialFilterStep;
          filterUniformBuffer.SubDataTyped(filterUniforms);
          Fwog::Cmd::BindSampledImage(0, indirectFilteredTex, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectUnfilteredTexPrev, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

          filterUniforms.stepWidth = 2 * spatialFilterStep;
          filterUniformBuffer.SubDataTyped(filterUniforms);
          Fwog::Cmd::BindSampledImage(0, indirectUnfilteredTexPrev, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectUnfilteredTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

          filterUniforms.stepWidth = 4 * spatialFilterStep;
          filterUniformBuffer.SubDataTyped(filterUniforms);
          Fwog::Cmd::BindSampledImage(0, indirectUnfilteredTex, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectFilteredTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

          filterUniforms.stepWidth = 8 * spatialFilterStep;
          filterUniformBuffer.SubDataTyped(filterUniforms);
          Fwog::Cmd::BindSampledImage(0, indirectFilteredTex, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectUnfilteredTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

          filterUniforms.stepWidth = 16 * spatialFilterStep;
          filterUniformBuffer.SubDataTyped(filterUniforms);
          Fwog::Cmd::BindSampledImage(0, indirectUnfilteredTex, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectFilteredTex, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
        }

        if (!rsmFilteredSkipAlbedoModulation)
        {
          Fwog::ScopedDebugMarker marker2("Modulate Albedo");
          Fwog::Cmd::BindComputePipeline(modulatePipeline);
          Fwog::Cmd::BindSampledImage(0, indirectFilteredTex, nearestSampler);
          Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectFilteredTexPingPong, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
          std::swap(indirectFilteredTex, indirectFilteredTexPingPong);
        }

      }
      else // Unfiltered RSM: the original paper
      {
        Fwog::Cmd::BindComputePipeline(rsmIndirectPipeline);
        Fwog::Cmd::BindSampledImage(0, indirectFilteredTex, nearestSampler);
        Fwog::Cmd::BindImage(0, indirectFilteredTex, 0);

        const int localSize = 8;
        const auto numGroups = (rsmUniforms.targetDim / 2 + localSize - 1) / localSize;

        // Quarter resolution indirect illumination pass
        uint32_t currentPass = 0;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

        // Reconstruction pass 1
        currentPass = 1;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

        // Reconstruction pass 2
        currentPass = 2;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);

        // Reconstruction pass 3
        currentPass = 3;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroups.x, numGroups.y, 1);
      }
    }
    Fwog::EndCompute();
  }

  const Fwog::Texture& RsmTechnique::GetIndirectLighting()
  {
    return indirectFilteredTex;
  }

  void RsmTechnique::DrawGui()
  {
    ImGui::Checkbox("Use Filtered RSM", &rsmFiltered);
    ImGui::SliderFloat("rMax", &rsmUniforms.rMax, 0.02f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
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
      ImGui::SliderFloat("Alpha Moments", &alphaMoments, epsilon, 1);
      ImGui::SliderFloat("Phi Luminance", &phiLuminance, epsilon, 1);
      ImGui::SliderFloat("Phi Normal", &phiNormal, epsilon, 1);
      ImGui::SliderFloat("Phi Depth", &phiDepth, epsilon, 1);
      ImGui::SliderFloat("Spatial Filter Step", &spatialFilterStep, 0, 1);
      ImGui::Checkbox("Skip Albedo Modulation", &rsmFilteredSkipAlbedoModulation);
      ImGui::Checkbox("Seed Each Frame", &seedEachFrame);
    }

    rsmUniforms.samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples);

    ImGui::PopButtonRepeat();
  }
} // namespace RSM
