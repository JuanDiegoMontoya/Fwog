#pragma once
#include <unordered_map>
#include <cstdint>
#include <cstddef>

namespace Fwog::detail
{
  struct VertexInputStateOwning;

  class VertexArrayCache
  {
  public:
    uint32_t CreateOrGetCachedVertexArray(const VertexInputStateOwning& inputState);
    size_t Size() const { return vertexArrayCache_.size(); }
    void Clear();

  private:
    std::unordered_map<size_t, uint32_t> vertexArrayCache_;
  };
}
