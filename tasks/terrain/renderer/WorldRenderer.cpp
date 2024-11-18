#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>

WorldRenderer::WorldRenderer(const etna::GpuWorkCount& workCount)
  : sceneMgr{std::make_unique<SceneManager>()}
  , modelMatrices{workCount, std::in_place_t()}

{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

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
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScenePrebaked(path);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {TERRAIN_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     TERRAIN_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("static_mesh", {TERRAIN_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
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
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });
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

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // draw final scene to screen
  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }
}
