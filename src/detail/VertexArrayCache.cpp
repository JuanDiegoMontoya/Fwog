#include "Fwog/detail/VertexArrayCache.h"
#include "Fwog/detail/Hash.h"
#include "Fwog/detail/ApiToEnum.h"
#include "Fwog/detail/PipelineManager.h"
#include "Fwog/Pipeline.h"
#include "Fwog/Common.h"

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
  }

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
      glEnableVertexArrayAttrib(vao, i);
      glVertexArrayAttribBinding(vao, i, desc.binding);

      auto type = detail::FormatToTypeGL(desc.format);
      auto size = detail::FormatToSizeGL(desc.format);
      auto normalized = detail::IsFormatNormalizedGL(desc.format);
      auto internalType = detail::FormatToFormatClass(desc.format);
      switch (internalType)
      {
      case detail::GlFormatClass::FLOAT:
        glVertexArrayAttribFormat(vao, i, size, type, normalized, desc.offset);
        break;
      case detail::GlFormatClass::INT:
        glVertexArrayAttribIFormat(vao, i, size, type, desc.offset);
        break;
      case detail::GlFormatClass::LONG:
        glVertexArrayAttribLFormat(vao, i, size, type, desc.offset);
        break;
      default: FWOG_UNREACHABLE;
      }
    }

    return vertexArrayCache_.insert({ inputHash, vao }).first->second;
  }

  void VertexArrayCache::Clear()
  {
    for (auto [_, vao] : vertexArrayCache_)
    {
      glDeleteVertexArrays(1, &vao);
    }

    vertexArrayCache_.clear();
  }
}

