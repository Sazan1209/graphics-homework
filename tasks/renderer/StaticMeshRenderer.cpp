#include "StaticMeshRenderer.hpp"
#include "scene/scene_manager/SceneLoader.hpp"
#include <etna/BlockingTransferHelper.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/Assert.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Sampler.hpp>
#include <etna/Profiling.hpp>
#include "shaders/static/static.h"


// Maybe these should just be one class, but I think it's more neat otherwise
static_assert(sizeof(REInstance) == sizeof(REInstanceCullingInfo));

static MaterialCompat makeCompat(const Material& src)
{
  return {
    .albedoIndex = src.albedoIndex,
    .normalIndex = src.normalIndex,
    .normalScale = src.normalScale,
    .albedoFactor = src.albedoFactor,
  };
}

void StaticMeshRenderer::loadScene(std::filesystem::path path)
{
  ZoneScopedN("loadStaticData");
  SceneData sceneDesc;
  {
    ZoneScopedN("loadSceneGeometry");
    SceneLoader loader;
    auto maybeSceneDesc = loader.selectScene(path);
    if (!maybeSceneDesc)
    {
      spdlog::error("Failed to load scene at {}", path.string());
      std::exit(1);
    }
    sceneDesc = std::move(*maybeSceneDesc);
  }
  {
    ZoneScopedN("transferStaticData");
    etna::BlockingTransferHelper transfer(etna::BlockingTransferHelper::CreateInfo{
      .stagingSize = 1024 * 1024,
    });
    std::unique_ptr<etna::OneShotCmdMgr> mgr = etna::get_context().createOneShotCmdMgr();

    singleRelemCount = sceneDesc.singleRelems.size();
    groupRelemCount = sceneDesc.groupedRelems.size();
    groupRelemInstanceCount = sceneDesc.groupedRelemsInstances.size();

    vertexDesc = sceneDesc.vertexDesc;

    auto& ctx = etna::get_context();

    relemMtWTransforms = ctx.createBuffer(etna::Buffer::CreateInfo{
      .size = sceneDesc.transforms.size() * sizeof(glm::mat4),
      .bufferUsage =
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      .name = "SM_Matrices",
    });
    transfer.uploadBuffer(
      *mgr, relemMtWTransforms, 0, std::span<const glm::mat4>(sceneDesc.transforms));

    if (singleRelemCount != 0)
    {
      std::vector<SingleREIndirectCommand> tmp(sceneDesc.singleRelems.size());
      singleRelemData = ctx.createBuffer(etna::Buffer::CreateInfo{
        .size = sceneDesc.singleRelems.size() * sizeof(SingleREIndirectCommand),
        .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        .name = "SM_SingleRelemDrawcalls",
      });

      for (size_t i = 0; i < sceneDesc.singleRelems.size(); ++i)
      {
        auto& src = sceneDesc.singleRelems[i];
        auto& dst = tmp[i];
        dst.command = {
          .indexCount = src.element.indexCount,
          .instanceCount = 0,
          .firstIndex = src.element.indexOffset,
          .vertexOffset = static_cast<int32_t>(src.element.vertexOffset),
          .firstInstance = static_cast<uint32_t>(i),
        };
        for (size_t i = 0; i < 3; ++i)
        {
          dst.max_coord[i] = src.element.box.posMax[i];
          dst.min_coord[i] = src.element.box.posMin[i];
        }
        dst.matrWfMIndex = src.matrixPos;
        dst.material = makeCompat(src.element.material);
      }
      transfer.uploadBuffer(
        *mgr, singleRelemData, 0, std::span<const SingleREIndirectCommand>(tmp));
    }
    if (groupRelemCount != 0)
    {
      std::vector<GroupREIndirectCommand> tmp(sceneDesc.groupedRelems.size());
      drawCallsGRE = ctx.createBuffer(etna::Buffer::CreateInfo{
        .size = sceneDesc.groupedRelems.size() * sizeof(GroupREIndirectCommand),
        .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        .name = "SM_GroupRelemDrawcalls",
      });

      for (size_t i = 0; i < sceneDesc.groupedRelems.size(); ++i)
      {
        auto& src = sceneDesc.groupedRelems[i];
        auto& dst = tmp[i];
        dst.command = {
          .indexCount = src.element.indexCount,
          .instanceCount = 0, // Instance count is calculated during runtime
          .firstIndex = src.element.indexOffset,
          .vertexOffset = static_cast<int32_t>(src.element.vertexOffset),
          .firstInstance = src.firstInstance,
        };
        for (size_t i = 0; i < 3; ++i)
        {
          dst.max_coord[i] = src.element.box.posMax[i];
          dst.min_coord[i] = src.element.box.posMin[i];
        }
        dst.material = makeCompat(src.element.material);
      }
      transfer.uploadBuffer(*mgr, drawCallsGRE, 0, std::span<const GroupREIndirectCommand>(tmp));

      cullingInfoGRE = ctx.createBuffer(etna::Buffer::CreateInfo{
        .size = sceneDesc.groupedRelemsInstances.size() * sizeof(REInstanceCullingInfo),
        .bufferUsage =
          vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        .name = "SM_InstanceCullingInfoDrawcalls",
      });
      transfer.uploadBuffer(
        *mgr, cullingInfoGRE, 0, std::span<const REInstance>(sceneDesc.groupedRelemsInstances));

      renderInfoGRE = ctx.createBuffer(etna::Buffer::CreateInfo{
        .size = sceneDesc.groupedRelemsInstances.size() * sizeof(REInstanceCullingInfo),
        .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        .name = "SM_InstanceRenderInfoDrawcalls",
      });
    }
    vertexData = std::move(sceneDesc.vertexData);
    indexData = std::move(sceneDesc.indexData);
    textures = std::move(sceneDesc.textures);
  }
  createDescSet();
}

void StaticMeshRenderer::loadShaders()
{
  ZoneScoped;
  etna::create_program(
    "reset_group_comms", {COMPLETE_RENDERER_SHADERS_ROOT "reset_group_comms.comp.spv"});
  etna::create_program(
    "single_sm_render",
    {COMPLETE_RENDERER_SHADERS_ROOT "render.frag.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "render_single.vert.spv"});
  etna::create_program(
    "group_sm_render",
    {COMPLETE_RENDERER_SHADERS_ROOT "render.frag.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "render_group.vert.spv"});
  etna::create_program("cull_single", {COMPLETE_RENDERER_SHADERS_ROOT "cull_single.comp.spv"});
  etna::create_program("cull_group", {COMPLETE_RENDERER_SHADERS_ROOT "cull_group.comp.spv"});
}

void StaticMeshRenderer::setupPipelines()
{
  ZoneScoped;
  auto& pipelineManager = etna::get_context().getPipelineManager();
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = vertexDesc,
    }},
  };
  singleRenderPipeline =
    pipelineManager.createGraphicsPipeline(
      "single_sm_render",
      etna::GraphicsPipeline::CreateInfo{
        .vertexShaderInput = sceneVertexInputDesc,
        .rasterizationConfig =
          vk::PipelineRasterizationStateCreateInfo{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .lineWidth = 1.f,
          },
        .blendingConfig =
          {.attachments =
             {vk::PipelineColorBlendAttachmentState{
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
              },
              vk::PipelineColorBlendAttachmentState{
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
              }},
           .logicOp = vk::LogicOp::eSet},
        .fragmentShaderOutput =
          {
            .colorAttachmentFormats =
              {vk::Format::eB10G11R11UfloatPack32, vk::Format::eA8B8G8R8SnormPack32},
            .depthAttachmentFormat = vk::Format::eD32Sfloat,
          },
      });
  groupRenderPipeline =
    pipelineManager.createGraphicsPipeline(
      "group_sm_render",
      etna::GraphicsPipeline::CreateInfo{
        .vertexShaderInput = sceneVertexInputDesc,
        .rasterizationConfig =
          vk::PipelineRasterizationStateCreateInfo{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .lineWidth = 1.f,
          },
        .blendingConfig =
          {.attachments =
             {vk::PipelineColorBlendAttachmentState{
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
              },
              vk::PipelineColorBlendAttachmentState{
                .blendEnable = vk::False,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
              }},
           .logicOp = vk::LogicOp::eSet},
        .fragmentShaderOutput =
          {
            .colorAttachmentFormats =
              {vk::Format::eB10G11R11UfloatPack32, vk::Format::eA8B8G8R8SnormPack32},
            .depthAttachmentFormat = vk::Format::eD32Sfloat,
          },
      });
  groupResetPipeline = pipelineManager.createComputePipeline("reset_group_comms", {});
  groupCullPipeline = pipelineManager.createComputePipeline("cull_group", {});
  singleCullPipeline = pipelineManager.createComputePipeline("cull_single", {});
}

void StaticMeshRenderer::renderScene(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderStatic);
  if (singleRelemCount != 0)
  {
    auto info = etna::get_shader_program("single_sm_render");
    auto binding0 = singleRelemData.genBinding();
    auto binding1 = relemMtWTransforms.genBinding();

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    auto& pipeline = singleRenderPipeline;
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
    cmd_buf.bindVertexBuffers(0, {vertexData.get()}, {0});
    cmd_buf.bindIndexBuffer(indexData.get(), 0, vk::IndexType::eUint32);
    cmd_buf.pushConstants<glm::mat4>(
      pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, {matrVfW});
    cmd_buf.drawIndexedIndirect(
      singleRelemData.get(),
      offsetof(SingleREIndirectCommand, command),
      singleRelemCount,
      sizeof(SingleREIndirectCommand));
  }
  if (groupRelemCount != 0)
  {
    auto info = etna::get_shader_program("group_sm_render");
    auto binding0 = renderInfoGRE.genBinding();
    auto binding1 = relemMtWTransforms.genBinding();

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    auto& pipeline = groupRenderPipeline;
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
    cmd_buf.bindVertexBuffers(0, {vertexData.get()}, {0});
    cmd_buf.bindIndexBuffer(indexData.get(), 0, vk::IndexType::eUint32);
    cmd_buf.pushConstants<glm::mat4>(
      pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, {matrVfW});
    cmd_buf.drawIndexedIndirect(
      drawCallsGRE.get(),
      offsetof(GroupREIndirectCommand, command),
      groupRelemCount,
      sizeof(GroupREIndirectCommand));
  }
}

void StaticMeshRenderer::prepareForRender(vk::CommandBuffer cmd_buf)
{
  if (singleRelemCount != 0)
  {
    ETNA_PROFILE_GPU(cmd_buf, cull_single);

    etna::set_state(
      cmd_buf,
      singleRelemData.get(),
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead);
    etna::flush_barriers(cmd_buf);

    auto info = etna::get_shader_program("cull_single");
    auto binding0 = singleRelemData.genBinding();
    auto binding1 = relemMtWTransforms.genBinding();


    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    auto& pipeline = singleCullPipeline;
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
    cmd_buf.pushConstants<glm::mat4>(
      pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, {matrVfW});
    cmd_buf.dispatch((singleRelemCount + 31) / 32, 1, 1);
    etna::set_state(
      cmd_buf,
      singleRelemData.get(),
      vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eVertexShader,
      vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eShaderStorageRead);
  }
  if (groupRelemInstanceCount != 0)
  {
    ETNA_PROFILE_GPU(cmd_buf, cull_group);
    etna::set_state(
      cmd_buf,
      drawCallsGRE.get(),
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite);
    etna::flush_barriers(cmd_buf);
    {
      auto info = etna::get_shader_program("reset_group_comms");
      auto binding0 = drawCallsGRE.genBinding();

      auto set = etna::create_descriptor_set(
        info.getDescriptorLayoutId(0),
        cmd_buf,
        {
          etna::Binding{0, binding0},
        });
      vk::DescriptorSet vkSet = set.getVkSet();
      auto& pipeline = groupResetPipeline;
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
      cmd_buf.dispatch((groupRelemCount + 31) / 32, 1, 1);
    }
    etna::set_state(
      cmd_buf,
      drawCallsGRE.get(),
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead);
    etna::set_state(
      cmd_buf,
      renderInfoGRE.get(),
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite);
    etna::flush_barriers(cmd_buf);
    {
      auto info = etna::get_shader_program("cull_group");
      auto binding0 = drawCallsGRE.genBinding();
      auto binding1 = relemMtWTransforms.genBinding();
      auto binding2 = renderInfoGRE.genBinding();
      auto binding3 = cullingInfoGRE.genBinding();

      auto set = etna::create_descriptor_set(
        info.getDescriptorLayoutId(0),
        cmd_buf,
        {
          etna::Binding{0, binding0},
          etna::Binding{1, binding1},
          etna::Binding{2, binding2},
          etna::Binding{3, binding3},
        });
      vk::DescriptorSet vkSet = set.getVkSet();
      auto& pipeline = groupCullPipeline;
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
      cmd_buf.pushConstants<glm::mat4>(
        pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, {matrVfW});
      cmd_buf.dispatch((groupRelemInstanceCount + 31) / 32, 1, 1);
    }
    etna::set_state(
      cmd_buf,
      drawCallsGRE.get(),
      vk::PipelineStageFlagBits2::eDrawIndirect,
      vk::AccessFlagBits2::eIndirectCommandRead);
    etna::set_state(
      cmd_buf,
      renderInfoGRE.get(),
      vk::PipelineStageFlagBits2::eVertexShader,
      vk::AccessFlagBits2::eShaderStorageRead);
  }
}

void StaticMeshRenderer::createDescSet()
{

  auto& ctx = etna::get_context();
  auto device = ctx.getDevice();
  {
    vk::DescriptorPoolSize poolSize{
      .type = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = MAX_DESCRIPTOR_NUM,
    };

    vk::DescriptorPoolCreateInfo info{
      .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
    };

    textureSetPool = etna::unwrap_vk_result(device.createDescriptorPool(info));
  }
  {
    auto binding = vk::DescriptorSetLayoutBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = MAX_DESCRIPTOR_NUM,
      .stageFlags = vk::ShaderStageFlagBits::eAll,
      .pImmutableSamplers = nullptr,
    };

    auto bindlessFlags = vk::DescriptorBindingFlags{
      vk::DescriptorBindingFlagBits::ePartiallyBound |
      vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    auto extendedInfo = vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT{
      .bindingCount = 1,
      .pBindingFlags = &bindlessFlags,
    };

    auto layoutInfo = vk::DescriptorSetLayoutCreateInfo{
      .pNext = &extendedInfo,
      .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPoolEXT,
      .bindingCount = 1,
      .pBindings = &binding,
    };

    textureSetLayout = etna::unwrap_vk_result(device.createDescriptorSetLayout(layoutInfo));
  }
  {
    uint32_t descriptorCount = static_cast<uint32_t>(MAX_DESCRIPTOR_NUM);

    vk::DescriptorSetVariableDescriptorCountAllocateInfo extraInfo{
      .descriptorSetCount = 1,
      .pDescriptorCounts = &descriptorCount,
    };

    vk::DescriptorSetAllocateInfo setAllocInfo{
      .pNext = &extraInfo,
      .descriptorPool = textureSetPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &textureSetLayout,
    };

    textureSet = etna::unwrap_vk_result(device.allocateDescriptorSets(setAllocInfo))[0];
  }
  {
    etna::Sampler sampler(etna::Sampler::CreateInfo{.name = "SM_DefaultSampler"});
    std::vector<vk::DescriptorImageInfo> imageInfos;
    imageInfos.reserve(textures.size());
    for (auto& texture : textures)
    {
      imageInfos.push_back(vk::DescriptorImageInfo{
        .sampler = sampler.get(),
        .imageView =
          texture.getView({0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers, {}, {}}),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
      });
    }

    vk::WriteDescriptorSet write{
      .dstSet = textureSet,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(textures.size()),
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .pImageInfo = imageInfos.data(),
    };

    device.updateDescriptorSets(1, &write, 0, nullptr);
  }
}

StaticMeshRenderer::~StaticMeshRenderer()
{
  auto device = etna::get_context().getDevice();
  device.destroyDescriptorPool(textureSetPool);
  device.destroyDescriptorSetLayout(textureSetLayout);
}
