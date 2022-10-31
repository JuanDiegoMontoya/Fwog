#include "RsmTechnique.h"
#include "Application.h"

#include <Fwog/DebugMarker.h>
#include <Fwog/Rendering.h>
#include <Fwog/Shader.h>

#include <imgui.h>

#include <stb_image.h>

#include <utility>

static Fwog::ComputePipeline CreateRsmIndirectPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, Application::LoadFile("shaders/RSMIndirect.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &cs});
}

static Fwog::ComputePipeline CreateRsmIndirectFilteredPipeline()
{
  auto cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER,
                         Application::LoadFile("shaders/RSMIndirectDitheredFiltered.comp.glsl"));

  return Fwog::ComputePipeline({.shader = &cs});
}

namespace RSM
{
  RsmTechnique::RsmTechnique(uint32_t width, uint32_t height)
    : rsmUniforms({
        .targetDim = {width, height},
        .rMax = rMax,
        .samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples),
      }),
      cameraUniformBuffer(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      rsmUniformBuffer(rsmUniforms, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      rsmIndirectPipeline(CreateRsmIndirectPipeline()),
      rsmIndirectFilteredPipeline(CreateRsmIndirectFilteredPipeline()),
      indirectLightingTex(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT)),
      indirectLightingTexPingPong(Fwog::CreateTexture2D({width, height}, Fwog::Format::R16G16B16A16_FLOAT))
  {
    // load blue noise texture
    int x = 0;
    int y = 0;
    auto noise = stbi_load("textures/bluenoise16.png", &x, &y, nullptr, 4);
    assert(noise);
    noiseTex = Fwog::CreateTexture2D({static_cast<uint32_t>(x), static_cast<uint32_t>(y)}, Fwog::Format::R8G8B8A8_UNORM);
    noiseTex->SubImage({
      .dimension = Fwog::UploadDimension::TWO,
      .level = 0,
      .offset = {},
      .size = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)},
      .format = Fwog::UploadFormat::RGBA,
      .type = Fwog::UploadType::UBYTE,
      .pixels = noise,
    });
    stbi_image_free(noise);
  }

  void RsmTechnique::ComputeIndirectLighting(const glm::mat4& lightViewProj,
                                             const CameraUniforms& cameraUniforms,
                                             const Fwog::Texture& gAlbedo,
                                             const Fwog::Texture& gNormal,
                                             const Fwog::Texture& gDepth,
                                             const Fwog::Texture& rsmFlux,
                                             const Fwog::Texture& rsmNormal,
                                             const Fwog::Texture& rsmDepth)
  {
    Fwog::SamplerState ss;
    ss.minFilter = Fwog::Filter::NEAREST;
    ss.magFilter = Fwog::Filter::NEAREST;
    ss.addressModeU = Fwog::AddressMode::REPEAT;
    ss.addressModeV = Fwog::AddressMode::REPEAT;
    auto nearestSampler = Fwog::Sampler(ss);

    rsmUniforms.sunViewProj = lightViewProj;
    rsmUniforms.invSunViewProj = glm::inverse(rsmUniforms.sunViewProj);
    rsmUniformBuffer.SubData(rsmUniforms, 0);

    cameraUniformBuffer.SubDataTyped(cameraUniforms);

    Fwog::BeginCompute();
    {
      Fwog::ScopedDebugMarker marker("Indirect Illumination");

      Fwog::Cmd::BindSampledImage(0, indirectLightingTex, nearestSampler);
      Fwog::Cmd::BindSampledImage(1, gAlbedo, nearestSampler);
      Fwog::Cmd::BindSampledImage(2, gNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(3, gDepth, nearestSampler);
      Fwog::Cmd::BindSampledImage(4, rsmFlux, nearestSampler);
      Fwog::Cmd::BindSampledImage(5, rsmNormal, nearestSampler);
      Fwog::Cmd::BindSampledImage(6, rsmDepth, nearestSampler);
      Fwog::Cmd::BindUniformBuffer(0, cameraUniformBuffer, 0, cameraUniformBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(1, rsmUniformBuffer, 0, rsmUniformBuffer.Size());
      Fwog::Cmd::BindImage(0, indirectLightingTex, 0);

      if (rsmFiltered)
      {
        Fwog::Cmd::BindComputePipeline(rsmIndirectFilteredPipeline);
        Fwog::Cmd::BindSampledImage(7, *noiseTex, nearestSampler);

        const int localSize = 8;
        const int numGroupsX = (rsmUniforms.targetDim.x + localSize - 1) / localSize;
        const int numGroupsY = (rsmUniforms.targetDim.y + localSize - 1) / localSize;

        uint32_t currentPass = 0;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT |
                                 Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);

        // Edge-avoiding a-trous (subsampled) filter pass
        {
          Fwog::ScopedDebugMarker marker2("Filter Subsampled");
          for (int i = 0; i < rsmFilterPasses; i++)
          {
            currentPass = 1;
            rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
            Fwog::Cmd::BindSampledImage(0, indirectLightingTex, nearestSampler);
            Fwog::Cmd::BindImage(0, indirectLightingTexPingPong, 0);
            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
            Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
            currentPass = 2;
            rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
            Fwog::Cmd::BindSampledImage(0, indirectLightingTexPingPong, nearestSampler);
            Fwog::Cmd::BindImage(0, indirectLightingTex, 0);
            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
            Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
          }
        }

        // Edge-avoiding box blur filter to clean up small details
        {
          Fwog::ScopedDebugMarker marker2("Filter Box");
          for (int i = 0; i < rsmBoxBlurPasses; i++)
          {
            currentPass = 3;
            rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
            Fwog::Cmd::BindSampledImage(0, indirectLightingTex, nearestSampler);
            Fwog::Cmd::BindImage(0, indirectLightingTexPingPong, 0);
            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
            Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
            currentPass = 4;
            rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
            Fwog::Cmd::BindSampledImage(0, indirectLightingTexPingPong, nearestSampler);
            Fwog::Cmd::BindImage(0, indirectLightingTex, 0);
            Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
            Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
          }
        }

        if (!rsmFilteredSkipAlbedoModulation)
        {
          Fwog::ScopedDebugMarker marker2("Modulate Albedo");
          currentPass = 5;
          rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
          Fwog::Cmd::BindSampledImage(0, indirectLightingTex, nearestSampler);
          Fwog::Cmd::BindImage(0, indirectLightingTexPingPong, 0);
          Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
          Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
          std::swap(indirectLightingTex, indirectLightingTexPingPong);
        }
      }
      else // Unfiltered RSM: the original paper
      {
        Fwog::Cmd::BindComputePipeline(rsmIndirectPipeline);

        const int localSize = 8;
        const int numGroupsX = (rsmUniforms.targetDim.x / 2 + localSize - 1) / localSize;
        const int numGroupsY = (rsmUniforms.targetDim.y / 2 + localSize - 1) / localSize;

        // Quarter resolution indirect illumination pass
        uint32_t currentPass = 0;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);

        // Reconstruction pass 1
        currentPass = 1;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);

        // Reconstruction pass 2
        currentPass = 2;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);

        // Reconstruction pass 3
        currentPass = 3;
        rsmUniformBuffer.SubData(currentPass, offsetof(RsmUniforms, currentPass));
        Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
        Fwog::Cmd::Dispatch(numGroupsX, numGroupsY, 1);
      }
    }
    Fwog::EndCompute();
  }

  const Fwog::Texture& RsmTechnique::GetIndirectLighting()
  {
    return indirectLightingTex;
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

      ImGui::InputInt("Filter Passes", &rsmFilterPasses, 1, 1);
      rsmFilterPasses = rsmFilterPasses < 0 ? 0 : rsmFilterPasses;
      ImGui::InputInt("Box Blur Passes", &rsmBoxBlurPasses, 1, 1);
      rsmBoxBlurPasses = rsmBoxBlurPasses < 0 ? 0 : rsmBoxBlurPasses;
      ImGui::Checkbox("Skip Albedo Modulation", &rsmFilteredSkipAlbedoModulation);
    }

    rsmUniforms.samples = static_cast<uint32_t>(rsmFiltered ? rsmFilteredSamples : rsmSamples);

    ImGui::PopButtonRepeat();
  }
} // namespace RSM
