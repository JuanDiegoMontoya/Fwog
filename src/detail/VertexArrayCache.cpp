#include "Fwog/detail/VertexArrayCache.h"
#include "Fwog/Pipeline.h"
#include "Fwog/detail/ApiToEnum.h"
#include "Fwog/detail/ContextState.h"
#include "Fwog/detail/Hash.h"
#include "Fwog/detail/PipelineManager.h"
#include FWOG_OPENGL_HEADER

namespace Fwog::detail
{
  namespace
  {
    size_t VertexInputStateHash(const VertexInputStateOwning& k)
    {
      size_t hashVal{};

      for (const auto& desc : k.vertexBindingDescriptions)
      {
        auto cctup = std::make_tuple(desc.location, desc.binding, desc.format, desc.offset);
        auto chashVal = Fwog::detail::hashing::hash<decltype(cctup)>{}(cctup);
        Fwog::detail::hashing::hash_combine(hashVal, chashVal);
      }

      return hashVal;
    }
  } // namespace

  uint32_t VertexArrayCache::CreateOrGetCachedVertexArray(const VertexInputStateOwning& inputState)
  {
    auto inputHash = VertexInputStateHash(inputState);
    if (auto it = vertexArrayCache_.find(inputHash); it != vertexArrayCache_.end())
    {
      return it->second;
    }

    uint32_t vao{};
    glCreateVertexArrays(1, &vao);
    for (uint32_t i = 0; i < inputState.vertexBindingDescriptions.size(); i++)
    {
      const auto& desc = inputState.vertexBindingDescriptions[i];
      glEnableVertexArrayAttrib(vao, desc.location);
      glVertexArrayAttribBinding(vao, desc.location, desc.binding);

      auto type = detail::FormatToTypeGL(desc.format);
      auto size = detail::FormatToSizeGL(desc.format);
      auto normalized = detail::IsFormatNormalizedGL(desc.format);
      auto internalType = detail::FormatToFormatClass(desc.format);
      switch (internalType)
      {
      case detail::GlFormatClass::FLOAT: glVertexArrayAttribFormat(vao, desc.location, size, type, normalized, desc.offset); break;
      case detail::GlFormatClass::INT: glVertexArrayAttribIFormat(vao, desc.location, size, type, desc.offset); break;
      case detail::GlFormatClass::LONG: glVertexArrayAttribLFormat(vao, desc.location, size, type, desc.offset); break;
      default: FWOG_UNREACHABLE;
      }
    }

    detail::InvokeVerboseMessageCallback("Created vertex array with handle ", vao);

    return vertexArrayCache_.insert({inputHash, vao}).first->second;
  }

  void VertexArrayCache::Clear()
  {
    for (auto [_, vao] : vertexArrayCache_)
    {
      detail::InvokeVerboseMessageCallback("Destroyed vertex array with handle ", vao);
      glDeleteVertexArrays(1, &vao);
    }

    vertexArrayCache_.clear();
  }
} // namespace Fwog::detail
