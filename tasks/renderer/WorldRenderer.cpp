#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include "shaders/tonemap/tonemap.h"

WorldRenderer::WorldRenderer(const etna::GpuWorkCount& workCount)
  : sceneMgr{std::make_unique<SceneManager>()}
  , modelMatrices{workCount, std::in_place_t()}
  , perlinSampler{etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "perlinSampler"}}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  mainView = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view",
    .format = vk::Format::eB10G11R11UfloatPack32,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage |
      vk::ImageUsageFlagBits::eTransferSrc,
  });

  mainViewDepth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  modelMatrices.iterate([&](auto& buf) {
    buf = ctx.createBuffer(etna::Buffer::CreateInfo{
      .size = sceneMgr->getInstanceMatrices().size_bytes(),
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      .name = "model_matrices",
    });

    buf.map();
  });

  perlinTex = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "perlin_noise",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  normalMap = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{4096, 4096, 1},
    .name = "normal_map",
    .format = vk::Format::eR8G8B8A8Snorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  tonemapHist = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = (tonemap_const::bucketCount + 2) * 4,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "tonemap_hist"});

  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createTerrainMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());

  cmdManager->submitAndWait(cmdBuf);
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
  etna::create_program("perlin", {COMPLETE_RENDERER_SHADERS_ROOT "perlin.comp.spv"});
  etna::create_program("normal", {COMPLETE_RENDERER_SHADERS_ROOT "normal.comp.spv"});
  etna::create_program(
    "terrain_render",
    {COMPLETE_RENDERER_SHADERS_ROOT "terrain.vert.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "terrain.frag.spv"});
  etna::create_program("tonemap_minmax", {COMPLETE_RENDERER_SHADERS_ROOT "minmax.comp.spv"});
  etna::create_program("tonemap_equalize", {COMPLETE_RENDERER_SHADERS_ROOT "equalize.comp.spv"});
  etna::create_program("tonemap_hist", {COMPLETE_RENDERER_SHADERS_ROOT "hist.comp.spv"});
  etna::create_program("tonemap_cumsum", {COMPLETE_RENDERER_SHADERS_ROOT "cumsum.comp.spv"});
  etna::create_program("tonemap", {COMPLETE_RENDERER_SHADERS_ROOT "tonemap.comp.spv"});
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
  staticMeshPipeline = pipelineManager.createGraphicsPipeline(
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
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eB10G11R11UfloatPack32},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });
  perlinPipeline = pipelineManager.createComputePipeline("perlin", {});
  normalPipeline = pipelineManager.createComputePipeline("normal", {});
  terrainPipeline = pipelineManager.createGraphicsPipeline(
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
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eB10G11R11UfloatPack32},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

  tonemapMinmaxPipeline = pipelineManager.createComputePipeline("tonemap_minmax", {});
  tonemapEqualizePipeline = pipelineManager.createComputePipeline("tonemap_equalize", {});
  tonemapHistPipeline = pipelineManager.createComputePipeline("tonemap_hist", {});
  tonemapCumsumPipeline = pipelineManager.createComputePipeline("tonemap_cumsum", {});
  tonemapPipeline = pipelineManager.createComputePipeline("tonemap", {});
}

void WorldRenderer::debugInput(const Keyboard&) {}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    nearPlane = packet.mainCam.zNear;
    farPlane = packet.mainCam.zFar;
    eye = packet.mainCam.position;
  }
}


bool WorldRenderer::shouldCull(glm::mat4 mModel, BoundingBox box)
{
  glm::mat4 mProj = worldViewProj * mModel;
  for (uint32_t mask = 0; mask < 8; ++mask)
  {
    glm::vec3 corner;
    for (uint32_t i = 0; i < 3; ++i)
    {
      corner[i] = (mask & (1u << i)) ? box.maxCoord[i] : box.minCoord[i];
    }
    glm::vec3 proj = mProj * glm::vec4(corner, 1);
    float xCos = abs(proj.x / sqrt(proj.x * proj.x + proj.z * proj.z));
    float yCos = abs(proj.y / sqrt(proj.y * proj.y + proj.z * proj.z));
    // Empirically, the magic constant here is 0.75. Not sure why, thought it was 0.5
    if (xCos <= 0.75 && yCos <= 0.75 && nearPlane <= proj.z && proj.z <= farPlane)
    {
      return false;
    }
  }
  return true;
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;

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

void WorldRenderer::tonemap(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, tonemap);
  vk::DependencyInfo depInfo{
    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
  };

  {
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

    cmd_buf.pipelineBarrier2(depInfo);
  }

  {
    auto minmaxInfo = etna::get_shader_program("tonemap_minmax");
    auto binding0 = mainView.genBinding({}, vk::ImageLayout::eGeneral, {});
    auto binding1 = tonemapHist.genBinding();
    auto set = etna::create_descriptor_set(
      minmaxInfo.getDescriptorLayoutId(0),
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
    cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
  }

  {
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
    auto binding0 = mainView.genBinding({}, vk::ImageLayout::eGeneral, {});
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
    cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
  }

  {
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
      .image = mainView.get(),
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
    auto binding0 = mainView.genBinding({}, vk::ImageLayout::eGeneral, {});
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
    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
  }
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  etna::set_state(
    cmd_buf,
    mainView.get(),
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = mainView.get(), .view = mainView.getView({})}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }

  {
    ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = mainView.get(),
        .view = mainView.getView({}),
        .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(),
       .view = mainViewDepth.getView({}),
       .loadOp = vk::AttachmentLoadOp::eLoad});
    renderTerrain(cmd_buf);
  }

  tonemap(cmd_buf);

  etna::set_state(
    cmd_buf,
    mainView.get(),
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
    mainView.get(),
    vk::ImageLayout::eTransferSrcOptimal,
    target_image,
    vk::ImageLayout::eTransferDstOptimal,
    1,
    &imageBlit,
    vk::Filter::eLinear);
}


void WorldRenderer::createTerrainMap(vk::CommandBuffer cmd_buf)
{
  {
    auto perlinInfo = etna::get_shader_program("perlin");

    auto binding = perlinTex.genBinding(perlinSampler.get(), vk::ImageLayout::eGeneral, {});

    // create desc set calls
    // etna::set_state(
    //   cmd_buf,
    //   perlinTex.get(),
    //   vk::PipelineStageFlagBits2::eComputeShader,
    //   vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageWrite,
    //   vk::ImageLayout::eGeneral,
    //   vk::ImageAspectFlagBits::eColor);

    auto set = etna::create_descriptor_set(
      perlinInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, perlinPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      perlinPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);


    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch(4096 / 32, 4096 / 32, 1);
  }

  {
    auto normalInfo = etna::get_shader_program("normal");
    auto binding0 = perlinTex.genBinding(perlinSampler.get(), vk::ImageLayout::eGeneral, {});
    auto binding1 = normalMap.genBinding(perlinSampler.get(), vk::ImageLayout::eGeneral, {});

    // Create descriptorSet will call
    // etna::set_state(
    //   cmd_buf,
    //   perlinTex.get(),
    //   vk::PipelineStageFlagBits2::eComputeShader,
    //   vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
    //   vk::ImageLayout::eGeneral,
    //   vk::ImageAspectFlagBits::eColor);

    // etna::set_state(
    //   cmd_buf,
    //   normalMap.get(),
    //   vk::PipelineStageFlagBits2::eComputeShader,
    //   vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
    //   vk::ImageLayout::eGeneral,
    //   vk::ImageAspectFlagBits::eColor);

    // the perlinTex barrier is dropped, so we call the following even though it's superfluous

    etna::set_state(
      cmd_buf,
      perlinTex.get(),
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageRead,
      vk::ImageLayout::eGeneral,
      vk::ImageAspectFlagBits::eColor);

    auto set = etna::create_descriptor_set(
      normalInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, normalPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      normalPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);

    etna::flush_barriers(cmd_buf);
    cmd_buf.dispatch(4096 / 32, 4096 / 32, 1);
  }

  etna::set_state(
    cmd_buf,
    perlinTex.get(),
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    normalMap.get(),
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);
}

void WorldRenderer::renderTerrain(vk::CommandBuffer cmd_buf)
{
  auto info = etna::get_shader_program("terrain_render");
  auto bind0 = perlinTex.genBinding(perlinSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
  auto bind1 = normalMap.genBinding(perlinSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

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
