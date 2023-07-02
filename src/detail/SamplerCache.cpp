#include "Fwog/detail/SamplerCache.h"
#include "Fwog/detail/ApiToEnum.h"
#include "Fwog/detail/Hash.h"
#include "Fwog/detail/ContextState.h"
#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  Sampler SamplerCache::CreateOrGetCachedTextureSampler(const SamplerState& samplerState)
  {
    if (auto it = samplerCache_.find(samplerState); it != samplerCache_.end())
    {
      return it->second;
    }

    uint32_t sampler{};
    glCreateSamplers(1, &sampler);

    glSamplerParameteri(sampler,
                        GL_TEXTURE_COMPARE_MODE,
                        samplerState.compareEnable ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);

    glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, detail::CompareOpToGL(samplerState.compareOp));

    GLint magFilter = samplerState.magFilter == Filter::LINEAR ? GL_LINEAR : GL_NEAREST;
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, magFilter);

    GLint minFilter{};
    switch (samplerState.mipmapFilter)
    {
    case Filter::NONE:
      minFilter = samplerState.minFilter == Filter::LINEAR ? GL_LINEAR : GL_NEAREST;
      break;
    case Filter::NEAREST:
      minFilter = samplerState.minFilter == Filter::LINEAR ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
      break;
    case Filter::LINEAR:
      minFilter = samplerState.minFilter == Filter::LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
      break;
    default: FWOG_UNREACHABLE;
    }
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, minFilter);

    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, detail::AddressModeToGL(samplerState.addressModeU));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, detail::AddressModeToGL(samplerState.addressModeV));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, detail::AddressModeToGL(samplerState.addressModeW));

    // TODO: determine whether int white values should be 1 or 255
    switch (samplerState.borderColor)
    {
    case BorderColor::FLOAT_TRANSPARENT_BLACK:
    {
      constexpr GLfloat color[4]{0, 0, 0, 0};
      glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    case BorderColor::INT_TRANSPARENT_BLACK:
    {
      constexpr GLint color[4]{0, 0, 0, 0};
      glSamplerParameteriv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    case BorderColor::FLOAT_OPAQUE_BLACK:
    {
      constexpr GLfloat color[4]{0, 0, 0, 1};
      glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    case BorderColor::INT_OPAQUE_BLACK:
    {
      // constexpr GLint color[4]{ 0, 0, 0, 255 };
      constexpr GLint color[4]{0, 0, 0, 1};
      glSamplerParameteriv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    case BorderColor::FLOAT_OPAQUE_WHITE:
    {
      constexpr GLfloat color[4]{1, 1, 1, 1};
      glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    case BorderColor::INT_OPAQUE_WHITE:
    {
      // constexpr GLint color[4]{ 255, 255, 255, 255 };
      constexpr GLint color[4]{1, 1, 1, 1};
      glSamplerParameteriv(sampler, GL_TEXTURE_BORDER_COLOR, color);
      break;
    }
    default: FWOG_UNREACHABLE; break;
    }

    glSamplerParameterf(sampler,
                        GL_TEXTURE_MAX_ANISOTROPY,
                        static_cast<GLfloat>(detail::SampleCountToGL(samplerState.anisotropy)));

    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, samplerState.lodBias);

    glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, samplerState.minLod);

    glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, samplerState.maxLod);

    detail::InvokeVerboseMessageCallback("Created sampler with handle ", sampler);

    return samplerCache_.insert({samplerState, Sampler(sampler)}).first->second;
  }

  size_t SamplerCache::Size() const
  {
    return samplerCache_.size();
  }

  void SamplerCache::Clear()
  {
    for (const auto& [_, sampler] : samplerCache_)
    {
      detail::InvokeVerboseMessageCallback("Destroyed sampler with handle ", sampler.id_);
      glDeleteSamplers(1, &sampler.id_);
    }

    samplerCache_.clear();
  }
} // namespace Fwog::detail

std::size_t std::hash<Fwog::SamplerState>::operator()(const Fwog::SamplerState& k) const noexcept
{
  auto rtup = std::make_tuple(k.minFilter,
                              k.magFilter,
                              k.mipmapFilter,
                              k.addressModeU,
                              k.addressModeV,
                              k.addressModeW,
                              k.borderColor,
                              k.anisotropy,
                              k.compareEnable,
                              k.compareOp,
                              k.lodBias,
                              k.minLod,
                              k.maxLod);
  return Fwog::detail::hashing::hash<decltype(rtup)>{}(rtup);
}
