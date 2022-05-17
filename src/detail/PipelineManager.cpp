#include <fwog/detail/PipelineManager.h>
#include <fwog/detail/Hash.h>
#include <unordered_map>

namespace Fwog::detail
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