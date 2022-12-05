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
    glm::vec3 viewDir;
    uint32_t _padding00;
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
    float rMax = 0.2f;
    float spatialFilterStep = 1.0f;
    float alphaIlluminance = 0.05f;
    float alphaMoments = 0.05f;
    float phiLuminance = 1.0f;
    float phiNormal = 0.3f;
    float phiDepth = 0.1f;
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
      uint32_t _padding00;
      glm::vec2 random;
    };

    struct ReprojectionUniforms
    {
      glm::mat4 invViewProjCurrent;
      glm::mat4 viewProjPrevious;
      glm::mat4 invViewProjPrevious;
      glm::mat4 proj;
      glm::vec3 viewPos;
      float temporalWeightFactor;
      glm::ivec2 targetDim;
      float alphaIlluminance;
      float alphaMoments;
      float phiDepth;
      float phiNormal;
      uint32_t _padding00;
      uint32_t _padding01;
    };

    struct FilterUniforms
    {
      glm::mat4 proj;
      glm::mat4 invViewProj;
      glm::vec3 viewPos;
      float stepWidth;
      glm::ivec2 targetDim;
      float phiLuminance;
      float phiNormal;
      float phiDepth;
      uint32_t _padding00;
      uint32_t _padding01;
      uint32_t _padding02;
    };

    glm::mat4 viewProjPrevious{1};
    glm::uint seedX;
    glm::uint seedY;
    RsmUniforms rsmUniforms;
    Fwog::TypedBuffer<RsmUniforms> rsmUniformBuffer;
    Fwog::TypedBuffer<CameraUniforms> cameraUniformBuffer;
    Fwog::TypedBuffer<ReprojectionUniforms> reprojectionUniformBuffer;
    Fwog::TypedBuffer<FilterUniforms> filterUniformBuffer;
    Fwog::ComputePipeline rsmIndirectPipeline;
    Fwog::ComputePipeline rsmIndirectFilteredPipeline;
    Fwog::ComputePipeline rsmReprojectPipeline;
    Fwog::ComputePipeline bilateral3x3Pipeline;
    Fwog::ComputePipeline bilateral5x5Pipeline;
    Fwog::ComputePipeline variancePipeline;
    Fwog::ComputePipeline modulatePipeline;
    Fwog::Texture indirectUnfilteredTex;
    Fwog::Texture indirectUnfilteredTexPrev; // for temporal accumulation
    Fwog::Texture indirectFilteredTex;
    Fwog::Texture indirectFilteredTexPingPong;
    Fwog::Texture historyLengthTex;
    Fwog::Texture momentsTex;
    Fwog::Texture momentsHistoryTex;
    Fwog::Texture varianceTex;
    std::optional<Fwog::Texture> noiseTex;
  };
} // namespace RSM