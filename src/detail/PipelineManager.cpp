#include <gsdf/detail/PipelineManager.h>
#include <unordered_map>

namespace hashing
{
  template<typename T> struct hash;

  template <class T>
  inline void hash_combine(std::size_t& seed, const T& v)
  {
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // Recursive template code derived from Matthieu M.
  template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
  struct HashValueImpl
  {
    static void apply(size_t& seed, const Tuple& tuple)
    {
      HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
      hash_combine(seed, std::get<Index>(tuple));
    }
  };

  template <class Tuple>
  struct HashValueImpl<Tuple, 0>
  {
    static void apply(size_t& seed, const Tuple& tuple)
    {
      hash_combine(seed, std::get<0>(tuple));
    }
  };

  template <typename ... TT>
  struct hash<std::tuple<TT...>>
  {
    size_t operator()(const std::tuple<TT...>& tt) const
    {
      size_t seed = 0;
      HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
      return seed;
    }
  };
}

namespace GFX::detail
{
  namespace
  {
    struct GraphicsPipelineHash
    {
      size_t operator()(const GraphicsPipeline& g) const
      {
        return g.id;
      }
    };
    struct ComputePipelineHash
    {
      size_t operator()(const ComputePipeline& p) const
      {
        return p.id;
      }
    };

    std::unordered_map<GraphicsPipeline, GraphicsPipelineInfoOwning, GraphicsPipelineHash> gGraphicsPipelines;
    std::unordered_map<ComputePipeline, ComputePipelineInfo, ComputePipelineHash> gComputePipelines;

    GraphicsPipeline HashPipelineInfo(const GraphicsPipelineInfo& info)
    {
      // hash all non-variable-size state
      auto rtup = std::make_tuple(
        info.shaderProgram,
        info.inputAssemblyState.primitiveRestartEnable,
        info.inputAssemblyState.topology,
        info.rasterizationState.depthClampEnable,
        info.rasterizationState.polygonMode,
        info.rasterizationState.cullMode,
        info.rasterizationState.frontFace,
        info.rasterizationState.depthBiasEnable,
        info.rasterizationState.depthBiasConstantFactor,
        info.rasterizationState.depthBiasSlopeFactor,
        info.rasterizationState.lineWidth,
        info.rasterizationState.pointSize,
        info.depthStencilState.depthTestEnable,
        info.depthStencilState.depthWriteEnable,
        info.depthStencilState.depthCompareOp,
        info.colorBlendState.logicOpEnable,
        info.colorBlendState.logicOp,
        info.colorBlendState.blendConstants[0],
        info.colorBlendState.blendConstants[1],
        info.colorBlendState.blendConstants[2],
        info.colorBlendState.blendConstants[3]
      );
      auto hashVal = hashing::hash<decltype(rtup)>{}(rtup);

      for (const auto& desc : info.vertexInputState.vertexBindingDescriptions)
      {
        auto dtup = std::make_tuple(
          desc.binding,
          desc.format,
          desc.location,
          desc.offset
        );
        auto dhashVal = hashing::hash<decltype(dtup)>{}(dtup);
        hashing::hash_combine(hashVal, dhashVal);
      }

      for (const auto& attachment : info.colorBlendState.attachments)
      {
        auto cctup = std::make_tuple(
          attachment.blendEnable,
          attachment.srcColorBlendFactor,
          attachment.dstColorBlendFactor,
          attachment.colorBlendOp,
          attachment.srcAlphaBlendFactor,
          attachment.dstAlphaBlendFactor,
          attachment.alphaBlendOp,
          attachment.colorWriteMask.flags
        );
        auto chashVal = hashing::hash<decltype(cctup)>{}(cctup);
        hashing::hash_combine(hashVal, chashVal);
      }

      return { hashVal };
    }
    GraphicsPipelineInfoOwning MakePipelineInfoOwning(const GraphicsPipelineInfo& info)
    {
      return GraphicsPipelineInfoOwning
      {
        .shaderProgram = info.shaderProgram,
        .inputAssemblyState = info.inputAssemblyState,
        .vertexInputState = { { info.vertexInputState.vertexBindingDescriptions.begin(), info.vertexInputState.vertexBindingDescriptions.end() } },
        .rasterizationState = info.rasterizationState,
        .depthStencilState = info.depthStencilState,
        .colorBlendState
        {
          .logicOpEnable = info.colorBlendState.logicOpEnable,
          .logicOp = info.colorBlendState.logicOp,
          .attachments = { info.colorBlendState.attachments.begin(), info.colorBlendState.attachments.end() },
          .blendConstants = { info.colorBlendState.blendConstants[0], info.colorBlendState.blendConstants[1], info.colorBlendState.blendConstants[2], info.colorBlendState.blendConstants[3] }
        }
      };
    }
  }

  std::optional<GraphicsPipeline> CompileGraphicsPipelineInternal(const GraphicsPipelineInfo& info)
  {
    auto pipeline = HashPipelineInfo(info);
    if (auto it = gGraphicsPipelines.find(pipeline); it != gGraphicsPipelines.end())
    {
      return it->first;
    }

    auto owning = MakePipelineInfoOwning(info);
    gGraphicsPipelines.insert({ pipeline, owning });
    return pipeline;
  }

  const GraphicsPipelineInfoOwning* GetGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    if (auto it = gGraphicsPipelines.find(pipeline); it != gGraphicsPipelines.end())
    {
      return &it->second;
    }
    return nullptr;
  }

  bool DestroyGraphicsPipelineInternal(GraphicsPipeline pipeline)
  {
    auto it = gGraphicsPipelines.find(pipeline);
    if (it == gGraphicsPipelines.end())
    {
      return false;
    }

    gGraphicsPipelines.erase(it);
    return true;
  }

  std::optional<ComputePipeline> CompileComputePipelineInternal(const ComputePipelineInfo& info)
  {
    ComputePipeline pipeline = { std::hash<decltype(info.shaderProgram)>{}(info.shaderProgram) };
    if (auto it = gComputePipelines.find(pipeline); it != gComputePipelines.end())
    {
      return it->first;
    }
    
    gComputePipelines.insert({ pipeline, info });
    return pipeline;
  }

  const ComputePipelineInfo* GetComputePipelineInternal(ComputePipeline pipeline)
  {
    if (auto it = gComputePipelines.find(pipeline); it != gComputePipelines.end())
    {
      return &it->second;
    }
    return nullptr;
  }

  bool DestroyComputePipelineInternal(ComputePipeline pipeline)
  {
    auto it = gComputePipelines.find(pipeline);
    if (it == gComputePipelines.end())
    {
      return false;
    }

    gComputePipelines.erase(it);
    return true;
  }
}