#pragma once
#include "gsdf/BasicTypes.h"
#include "gsdf/Texture.h"
#include <unordered_map>

namespace std
{
  template<>
  struct hash<GFX::SamplerState>
  {
    std::size_t operator()(const GFX::SamplerState& k) const;
  };
}

namespace GFX::detail
{
  class SamplerCache
  {
  public:
    TextureSampler CreateOrGetCachedTextureSampler(const SamplerState& samplerState);
    size_t Size() const;
    void Clear();

  private:
    std::unordered_map<SamplerState, TextureSampler> samplerCache_;
  };
}
