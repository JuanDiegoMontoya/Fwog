#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace Fwog::detail
{
  struct VertexInputStateOwning;

  class VertexArrayCache
  {
  public:
    VertexArrayCache() = default;
    VertexArrayCache(const VertexArrayCache&) = delete;
    VertexArrayCache& operator=(const VertexArrayCache&) = delete;
    VertexArrayCache(VertexArrayCache&&) noexcept = default;
    VertexArrayCache& operator=(VertexArrayCache&&) noexcept = default;

    ~VertexArrayCache()
    {
      Clear();
    }

    uint32_t CreateOrGetCachedVertexArray(const VertexInputStateOwning& inputState);

    [[nodiscard]] size_t Size() const
    {
      return vertexArrayCache_.size();
    }

    void Clear();

  private:
    std::unordered_map<size_t, uint32_t> vertexArrayCache_;
  };
} // namespace Fwog::detail
