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
    glm::vec2 jitterOffset{};
    glm::vec2 lastFrameJitterOffset{};
  };

  class RsmTechnique
  {
  public:
    RsmTechnique(uint32_t width, uint32_t height);

    void SetResolution(uint32_t newWidth, uint32_t newHeight);

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
                                 const Fwog::Texture& gNormalPrev,
                                 const Fwog::Texture& gMotion);

    Fwog::Texture& GetIndirectLighting();

    void DrawGui();

    int inverseResolutionScale = 1;
    int smallRsmSize = 512;
    int rsmSamples = 400;
    int rsmFilteredSamples = 8;
    float rMax = 0.2f;
    float spatialFilterStep = 1.0f;
    float alphaIlluminance = 0.05f;
    float phiNormal = 0.3f;
    float phiDepth = 0.2f;
    bool rsmFiltered = true;
    bool rsmFilteredSkipAlbedoModulation = false;
    bool seedEachFrame = true;
    bool useSeparableFilter = true;

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
      float phiDepth;
      float phiNormal;
      uint32_t _padding00;
      glm::vec2 jitterOffset;
      glm::vec2 lastFrameJitterOffset;
    };

    struct FilterUniforms
    {
      glm::mat4 proj;
      glm::mat4 invViewProj;
      glm::vec3 viewPos;
      float stepWidth;
      glm::ivec2 targetDim;
      glm::ivec2 direction;
      float phiNormal;
      float phiDepth;
      uint32_t _padding00;
      uint32_t _padding01;
    };

    uint32_t width;
    uint32_t height;
    uint32_t internalWidth;
    uint32_t internalHeight;
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
    Fwog::ComputePipeline bilateral5x5Pipeline;
    Fwog::ComputePipeline modulatePipeline;
    Fwog::ComputePipeline modulateUpscalePipeline;
    Fwog::ComputePipeline blitPipeline;
    std::optional<Fwog::Texture> indirectUnfilteredTex;
    std::optional<Fwog::Texture> indirectUnfilteredTexPrev; // for temporal accumulation
    std::optional<Fwog::Texture> indirectFilteredTex;
    std::optional<Fwog::Texture> indirectFilteredTexPingPong;
    std::optional<Fwog::Texture> historyLengthTex;
    std::optional<Fwog::Texture> illuminationUpscaled;
    std::optional<Fwog::Texture> rsmFluxSmall;
    std::optional<Fwog::Texture> rsmNormalSmall;
    std::optional<Fwog::Texture> rsmDepthSmall;
    std::optional<Fwog::Texture> noiseTex;
    std::optional<Fwog::Texture> gNormalSmall;
    std::optional<Fwog::Texture> gDepthSmall;
    std::optional<Fwog::Texture> gNormalPrevSmall;
    std::optional<Fwog::Texture> gDepthPrevSmall;
  };
} // namespace RSM