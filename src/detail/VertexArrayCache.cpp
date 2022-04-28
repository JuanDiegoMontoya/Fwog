#include "gsdf/detail/VertexArrayCache.h"
#include "gsdf/detail/Hash.h"
#include "gsdf/detail/ApiToEnum.h"
#include "gsdf/detail/PipelineManager.h"
#include "gsdf/Pipeline.h"
#include "gsdf/Common.h"

namespace GFX::detail
{
  namespace
  {
    size_t VertexInputStateHash(const VertexInputStateOwning& k)
    {
      size_t hashVal{};
      
      for (const auto& desc : k.vertexBindingDescriptions)
      {
        auto cctup = std::make_tuple(desc.location, desc.binding, desc.format, desc.offset);
        auto chashVal = GFX::detail::hashing::hash<decltype(cctup)>{}(cctup);
        GFX::detail::hashing::hash_combine(hashVal, chashVal);
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
      default: GSDF_UNREACHABLE;
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

