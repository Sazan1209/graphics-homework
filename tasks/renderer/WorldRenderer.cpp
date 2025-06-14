#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include "shaders/tonemap/tonemap.h"
#include "shaders/resolve.h"
#include "shaders/terrain/terrain.h"
#include "TerrainGenerator.hpp"
#include <imgui.h>

WorldRenderer::WorldRenderer(const etna::GpuWorkCount& workCount)
  : sceneMgr{std::make_unique<SceneManager>()}
  , modelMatrices{workCount, std::in_place_t()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();
  defaultSampler = etna::Sampler(
    etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "default_sampler"});

  gBuffer.color = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer.color",
    .format = vk::Format::eB10G11R11UfloatPack32,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage |
      vk::ImageUsageFlagBits::eTransferSrc,
  });

  gBuffer.depthStencil = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer.depthStencil",
    .format = vk::Format::eD32Sfloat,
    .imageUsage =
      vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eStorage,
  });

  gBuffer.normal = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer.normal",
    .format = vk::Format::eA8B8G8R8SnormPack32,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage});

  modelMatrices.iterate([&](auto& buf) {
    buf = ctx.createBuffer(etna::Buffer::CreateInfo{
      .size = sceneMgr->getInstanceMatrices().size_bytes(),
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      .name = "model_matrices",
    });

    buf.map();
  });

  {
    // todo fix this
    // double aspect = double(resolution.x) / double(resolution.y);
    uint32_t yLen = 45; // static_cast<uint32_t>(glm::ceil(2 * tanFov / 0.01745));
    uint32_t xLen = 80; // static_cast<uint32_t>(glm::ceil(2 * tanFov * aspect / 0.01745));
    downscaledRes = glm::uvec2(xLen, yLen);

    tonemapDownscaledImage = ctx.createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{xLen, yLen, 1},
      .name = "tonemap_image_downscaled",
      .format = vk::Format::eB10G11R11UfloatPack32,
      .imageUsage = vk::ImageUsageFlagBits::eStorage});
  }

  tonemapHist = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = (tonemap_const::bucketCount + 2) * 4,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_TO_CPU,
    .name = "tonemap_hist"});
  tonemapHist.map();

  resolveUniformParamsBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(resolveUniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "resolve_uniform_params"});
  resolveUniformParamsBuffer.map();

  TerrainGenerator generator;
  auto terrainInfo = generator.generate();
  heightMap = std::move(terrainInfo.heightMap);
  normalMap = std::move(terrainInfo.normalMap);
  lightList = std::move(terrainInfo.lightList);
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScenePrebaked(path);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {COMPLETE_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program(
    "terrain_render",
    {COMPLETE_RENDERER_SHADERS_ROOT "terrain.vert.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.frag.spv"});
  etna::create_program("tonemap_downscale", {COMPLETE_RENDERER_SHADERS_ROOT "downscale.comp.spv"});
  etna::create_program("tonemap_minmax", {COMPLETE_RENDERER_SHADERS_ROOT "minmax.comp.spv"});
  etna::create_program("tonemap_equalize", {COMPLETE_RENDERER_SHADERS_ROOT "equalize.comp.spv"});
  etna::create_program("tonemap_hist", {COMPLETE_RENDERER_SHADERS_ROOT "hist.comp.spv"});
  etna::create_program("tonemap_cumsum", {COMPLETE_RENDERER_SHADERS_ROOT "cumsum.comp.spv"});
  etna::create_program("tonemap", {COMPLETE_RENDERER_SHADERS_ROOT "tonemap.comp.spv"});
  etna::create_program("resolve", {COMPLETE_RENDERER_SHADERS_ROOT "resolve.comp.spv"});
  etna::create_program(
    "cube",
    {COMPLETE_RENDERER_SHADERS_ROOT "cube.frag.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "cube.vert.spv"});
}

void WorldRenderer::setupPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  staticMeshPipeline = {};
  staticMeshPipeline =
    pipelineManager.createGraphicsPipeline(
      "static_mesh_material",
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
  cubePipeline =
    pipelineManager.createGraphicsPipeline(
      "cube",
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
  terrainPipeline =
    pipelineManager.createGraphicsPipeline(
      "terrain_render",
      etna::GraphicsPipeline::CreateInfo{
        .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
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

  tonemapDownscalePipeline = pipelineManager.createComputePipeline("tonemap_downscale", {});
  tonemapMinmaxPipeline = pipelineManager.createComputePipeline("tonemap_minmax", {});
  tonemapEqualizePipeline = pipelineManager.createComputePipeline("tonemap_equalize", {});
  tonemapHistPipeline = pipelineManager.createComputePipeline("tonemap_hist", {});
  tonemapCumsumPipeline = pipelineManager.createComputePipeline("tonemap_cumsum", {});
  tonemapPipeline = pipelineManager.createComputePipeline("tonemap", {});

  resolvePipeline = pipelineManager.createComputePipeline("resolve", {});
}

void WorldRenderer::debugInput(const Keyboard&) {}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    resolveUniformParams.near = packet.mainCam.zNear;
    resolveUniformParams.far = packet.mainCam.zFar;
    resolveUniformParams.tanFov = glm::tan(glm::radians(packet.mainCam.fov) / 2.0f);
    resolveUniformParams.mView = packet.mainCam.viewTm();
    eye = packet.mainCam.position;
  }
}

// todo redo
bool WorldRenderer::shouldCull(glm::mat4, BoundingBox)
{
  return false;
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Menu");
  if (ImGui::CollapsingHeader("Tonemapping"))
  {
    ImGui::PlotHistogram("Adjusted hist", (float*)tonemapHist.data() + 2, 128);
    ImGui::Value("Min lum", ((float*)tonemapHist.data())[0]);
    ImGui::Value("Max lum", ((float*)tonemapHist.data())[1]);
    bool tmp = tonemapPushConstants.forceLinear;
    if (ImGui::Checkbox("Force linear tonemapping", &tmp))
    {
      tonemapPushConstants.forceLinear = tmp ? 1 : 0;
    }
  }
  if (ImGui::CollapsingHeader("Lighting"))
  {
    ImGui::InputFloat("Attenuation coefficient", &resolveUniformParams.attenuationCoef);
    ImGui::InputFloat("Specular exponent", &resolveUniformParams.lightExponent);
    ImGui::InputFloat("Ambient", &resolveUniformParams.sunlight.ambient);
  }
  if (ImGui::CollapsingHeader("Sunlight"))
  {
    if (ImGui::DragFloat2("Angle", sunlightAngles, 1.0f, -89.0f, 89.0f))
    {
      float a0 = glm::radians(sunlightAngles[0]);
      float a1 = glm::radians(sunlightAngles[1]);
      float x = glm::sin(a1) * glm::cos(a0);
      float y = -glm::cos(a0) * glm::cos(a1);
      float z = -glm::sin(a0) * glm::cos(a1);

      resolveUniformParams.sunlight.dir = glm::normalize(glm::vec3(x, y, z));
    };
    ImGui::InputFloat("Strength", &resolveUniformParams.sunlight.strength);
    ImGui::ColorPicker3("Color", (float*)&resolveUniformParams.sunlight.color);
  }
  ImGui::End();
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = gBuffer.color.get(), .view = gBuffer.color.getView({})},
       {.image = gBuffer.normal.get(), .view = gBuffer.normal.getView({})}},
      {.image = gBuffer.depthStencil.get(), .view = gBuffer.depthStencil.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
    renderTerrain(cmd_buf);
    renderCube(cmd_buf);
  }

  resolve(cmd_buf);

  tonemap(cmd_buf);

  etna::set_state(
    cmd_buf,
    gBuffer.color.get(),
    vk::PipelineStageFlagBits2::eBlit,
    vk::AccessFlagBits2::eTransferRead,
    vk::ImageLayout::eTransferSrcOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    target_image,
    vk::PipelineStageFlagBits2::eBlit,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eTransferDstOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::flush_barriers(cmd_buf);

  std::array offsets = {
    vk::Offset3D{},
    vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1}};

  auto imageBlit = vk::ImageBlit{
    .srcSubresource =
      vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1},
    .srcOffsets = offsets,
    .dstSubresource =
      vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1},
    .dstOffsets = offsets};

  cmd_buf.blitImage(
    gBuffer.color.get(),
    vk::ImageLayout::eTransferSrcOptimal,
    target_image,
    vk::ImageLayout::eTransferDstOptimal,
    1,
    &imageBlit,
    vk::Filter::eLinear);
}


void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;
  ETNA_PROFILE_GPU(cmd_buf, renderScene);

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);
  {
    auto info = etna::get_shader_program("static_mesh_material");

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0), cmd_buf, {etna::Binding{0, modelMatrices.get().genBinding()}});
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, 1, &vkSet, 0, nullptr);
  }


  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatrices = sceneMgr->getInstanceMatrices();

  auto meshes = sceneMgr->getMeshes();
  auto relems = sceneMgr->getRenderElements();

  for (std::size_t instIdx = 0; instIdx < instanceMeshes.size(); ++instIdx)
  {
    const auto meshIdx = instanceMeshes[instIdx];

    std::size_t nextIdx = instIdx;
    std::size_t nonCulled = 0;
    while (nextIdx < instanceMeshes.size() && instanceMeshes[nextIdx] == meshIdx)
    {
      if (!shouldCull(instanceMatrices[nextIdx], sceneMgr->getMeshes()[meshIdx].box))
      {
        std::memcpy(
          modelMatrices.get().data() + (nonCulled + instIdx) * sizeof(glm::mat4),
          &instanceMatrices[nextIdx],
          sizeof(glm::mat4));
        nonCulled += 1;
      }
      ++nextIdx;
    }

    cmd_buf.pushConstants<glm::mat4>(pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, glob_tm);

    for (uint32_t j = 0; j < meshes[meshIdx].relemCount; ++j)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      const auto& relem = relems[relemIdx];
      cmd_buf.drawIndexed(
        relem.indexCount,
        static_cast<uint32_t>(nonCulled),
        relem.indexOffset,
        relem.vertexOffset,
        static_cast<uint32_t>(instIdx));
    }
    instIdx = nextIdx - 1;
  }
}

void WorldRenderer::renderTerrain(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
  auto info = etna::get_shader_program("terrain_render");
  auto bind0 = heightMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
  auto bind1 = normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

  auto descSet = etna::create_descriptor_set(
    info.getDescriptorLayoutId(0), cmd_buf, {etna::Binding{0, bind0}, etna::Binding{1, bind1}});
  auto vkSet = descSet.getVkSet();
  auto layout = terrainPipeline.getVkPipelineLayout();

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &vkSet, 0, nullptr);

  cmd_buf.pushConstants<TerrainPushConst>(
    layout,
    vk::ShaderStageFlagBits::eTessellationEvaluation |
      vk::ShaderStageFlagBits::eTessellationControl,
    0,
    {TerrainPushConst{worldViewProj, eye}});

  // etna::flush_barriers(cmd_buf);
  cmd_buf.draw(3, (4096 * 4096) / (128 * 128), 0, 0);
}

void WorldRenderer::renderCube(vk::CommandBuffer cmd_buf)
{

  ETNA_PROFILE_GPU(cmd_buf, cube);
  auto info = etna::get_shader_program("cube");
  auto bind0 = lightList.genBinding();

  auto descSet =
    etna::create_descriptor_set(info.getDescriptorLayoutId(0), cmd_buf, {etna::Binding{0, bind0}});
  auto vkSet = descSet.getVkSet();
  auto layout = cubePipeline.getVkPipelineLayout();

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, cubePipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &vkSet, 0, nullptr);

  cmd_buf.pushConstants<glm::mat4>(layout, vk::ShaderStageFlagBits::eVertex, 0, worldViewProj);

  cmd_buf.draw(36, 64, 0, 0);
}


void WorldRenderer::resolve(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, resolve);
  std::memcpy(
    resolveUniformParamsBuffer.data(), &resolveUniformParams, sizeof(resolveUniformParams));

  auto info = etna::get_shader_program("resolve");
  auto binding0 = gBuffer.color.genBinding({}, vk::ImageLayout::eGeneral, {});
  auto binding1 = gBuffer.depthStencil.genBinding({}, vk::ImageLayout::eGeneral, {});
  auto binding2 = gBuffer.normal.genBinding({}, vk::ImageLayout::eGeneral, {});
  auto binding3 = lightList.genBinding();
  auto binding4 = resolveUniformParamsBuffer.genBinding();
  auto set = etna::create_descriptor_set(
    info.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, binding0},
      etna::Binding{1, binding1},
      etna::Binding{2, binding2},
      etna::Binding{3, binding3},
      etna::Binding{4, binding4},
    });
  vk::DescriptorSet vkSet = set.getVkSet();
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, resolvePipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    resolvePipeline.getVkPipelineLayout(),
    0,
    1,
    &vkSet,
    0,
    nullptr);
  etna::flush_barriers(cmd_buf);
  cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
}

void WorldRenderer::tonemap(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, tonemap);

  // Possibly transfer layout for imageDownscaled

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  cmd_buf.fillBuffer(tonemapHist.get(), 0, VK_WHOLE_SIZE, 0);

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::ImageMemoryBarrier2 barrierImg{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask =
        vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask =
        vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
      .oldLayout = vk::ImageLayout::eGeneral,
      .newLayout = vk::ImageLayout::eGeneral,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = gBuffer.color.get(),
      .subresourceRange =
        {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .baseMipLevel = 0,
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .baseArrayLayer = 0,
          .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrierImg;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto info = etna::get_shader_program("tonemap_downscale");
    auto binding0 = gBuffer.color.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto binding1 = tonemapDownscaledImage.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapDownscalePipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapDownscalePipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch(downscaledRes.x, downscaledRes.y, 1);
  }

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    vk::ImageMemoryBarrier2 barrierImg{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .oldLayout = vk::ImageLayout::eGeneral,
      .newLayout = vk::ImageLayout::eGeneral,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = tonemapDownscaledImage.get(),
      .subresourceRange =
        {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .baseMipLevel = 0,
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .baseArrayLayer = 0,
          .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrierImg;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto info = etna::get_shader_program("tonemap_minmax");
    auto binding0 = tonemapDownscaledImage.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto binding1 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapMinmaxPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapMinmaxPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch((downscaledRes.x + 31) / 32, (downscaledRes.y + 31) / 32, 1);
  }

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto histInfo = etna::get_shader_program("tonemap_hist");
    auto binding0 = tonemapDownscaledImage.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto binding1 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      histInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapHistPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapHistPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch((downscaledRes.x + 31) / 32, (downscaledRes.y + 31) / 32, 1);
  }

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto info = etna::get_shader_program("tonemap_equalize");
    auto binding0 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapEqualizePipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapEqualizePipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch(1, 1, 1);
  }

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto cumsumInfo = etna::get_shader_program("tonemap_cumsum");
    auto binding0 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      cumsumInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapCumsumPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapCumsumPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch(1, 1, 1); // beautiful
  }

  {
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };
    vk::BufferMemoryBarrier2 barrierBuf = {
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = tonemapHist.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE};

    vk::ImageMemoryBarrier2 barrierImg{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .oldLayout = vk::ImageLayout::eGeneral,
      .newLayout = vk::ImageLayout::eGeneral,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = gBuffer.color.get(),
      .subresourceRange =
        {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .baseMipLevel = 0,
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .baseArrayLayer = 0,
          .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrierImg;

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &barrierBuf;

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto tonemapInfo = etna::get_shader_program("tonemap");
    auto binding0 = gBuffer.color.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto binding1 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      tonemapInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });
    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, tonemapPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      tonemapPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);
    cmd_buf.pushConstants<decltype(tonemapPushConstants)>(
      tonemapPipeline.getVkPipelineLayout(),
      vk::ShaderStageFlagBits::eCompute,
      0,
      tonemapPushConstants);
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
  }
}
