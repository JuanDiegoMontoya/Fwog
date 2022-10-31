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

    // Input: camera uniforms, g-buffers, RSM buffers
    void ComputeIndirectLighting(const glm::mat4& lightViewProj,
                                 const CameraUniforms& cameraUniforms,
                                 const Fwog::Texture& gAlbedo,
                                 const Fwog::Texture& gNormal,
                                 const Fwog::Texture& gDepth,
                                 const Fwog::Texture& rsmFlux,
                                 const Fwog::Texture& rsmNormal,
                                 const Fwog::Texture& rsmDepth);

    const Fwog::Texture& GetIndirectLighting();

    void DrawGui();

    int rsmSamples = 400;
    int rsmFilteredSamples = 15;
    int rsmFilterPasses = 2;
    int rsmBoxBlurPasses = 1;
    float rMax = 0.08f;
    bool rsmFiltered = false;
    bool rsmFilteredSkipAlbedoModulation = false;

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
    };

    RsmUniforms rsmUniforms;
    Fwog::TypedBuffer<RsmUniforms> rsmUniformBuffer;
    Fwog::TypedBuffer<CameraUniforms> cameraUniformBuffer;
    Fwog::ComputePipeline rsmIndirectPipeline;
    Fwog::ComputePipeline rsmIndirectFilteredPipeline;
    Fwog::Texture indirectLightingTex;
    Fwog::Texture indirectLightingTexPingPong;
    std::optional<Fwog::Texture> noiseTex;
  };
} // namespace RSM