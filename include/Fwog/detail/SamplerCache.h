#pragma once
#include "Fwog/BasicTypes.h"
#include "Fwog/Texture.h"
#include <unordered_map>

template<>
struct std::hash<Fwog::SamplerState>
{
  std::size_t operator()(const Fwog::SamplerState& k) const noexcept;
};

namespace Fwog::detail
{
  class SamplerCache
  {
  public:
    SamplerCache() = default;
    SamplerCache(const SamplerCache&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;
    SamplerCache(SamplerCache&&) noexcept = default;
    SamplerCache& operator=(SamplerCache&&) noexcept = default;
    ~SamplerCache()
    {
      Clear();
    }

    Sampler CreateOrGetCachedTextureSampler(const SamplerState& samplerState);
    [[nodiscard]] size_t Size() const;
    void Clear();

  private:
    std::unordered_map<SamplerState, Sampler> samplerCache_;
  };
} // namespace Fwog::detail