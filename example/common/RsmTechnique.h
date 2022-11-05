#pragma once
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Texture.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <optional>

namespace RSM
{
  struct CameraUniforms
  {
    glm::mat4 viewProj;
    glm::mat4 invViewProj;
    glm::mat4 proj;
    glm::vec4 cameraPos;
  };

  class RsmTechnique
  {
  public:
    RsmTechnique(uint32_t width, uint32_t height);

    // Input: camera uniforms, g-buffers, RSM buffers, previous g-buffer depth (for reprojection)
    void ComputeIndirectLighting(const glm::mat4& lightViewProj,
                                 const CameraUniforms& cameraUniforms,
                                 const Fwog::Texture& gAlbedo,
                                 const Fwog::Texture& gNormal,
                                 const Fwog::Texture& gDepth,
                                 const Fwog::Texture& rsmFlux,
                                 const Fwog::Texture& rsmNormal,
                                 const Fwog::Texture& rsmDepth,
                                 const Fwog::Texture& gDepthPrev,
                                 const Fwog::Texture& gNormalPrev);

    const Fwog::Texture& GetIndirectLighting();

    void DrawGui();

    int rsmSamples = 400;
    int rsmFilteredSamples = 1;
    int rsmFilterPasses = 1;
    int rsmBoxBlurPasses = 1;
    float rMax = 0.2f;
    float temporalAlpha = 0;
    bool rsmFiltered = false;
    bool rsmFilteredSkipAlbedoModulation = false;
    bool seedEachFrame = true;

  private:
    struct RsmUniforms
    {
      glm::mat4 sunViewProj;
      glm::mat4 invSunViewProj;
      glm::ivec2 targetDim;
      float rMax;
      uint32_t currentPass;
      uint32_t samples;
      float random;
      uint32_t _padding00;
      uint32_t _padding01;
    };

    struct ReprojectionUniforms
    {
      glm::mat4 invViewProjCurrent;
      glm::mat4 viewProjPrevious;
      glm::mat4 invViewProjPrevious;
      glm::ivec2 targetDim;
      float temporalWeightFactor;
      glm::uint _padding00;
    };

    glm::mat4 viewProjPrevious{1};
    glm::uint seed;
    RsmUniforms rsmUniforms;
    Fwog::TypedBuffer<RsmUniforms> rsmUniformBuffer;
    Fwog::TypedBuffer<CameraUniforms> cameraUniformBuffer;
    Fwog::TypedBuffer<ReprojectionUniforms> reprojectionUniformBuffer;
    Fwog::ComputePipeline rsmIndirectPipeline;
    Fwog::ComputePipeline rsmIndirectFilteredPipeline;
    Fwog::ComputePipeline rsmReprojectPipeline;
    Fwog::Texture indirectUnfilteredTex;
    Fwog::Texture indirectUnfilteredTexPrev; // for temporal accumulation
    Fwog::Texture indirectFilteredTex;
    Fwog::Texture indirectFilteredTexPingPong;
    Fwog::Texture historyLengthTex;
    std::optional<Fwog::Texture> noiseTex;
  };
} // namespace RSM