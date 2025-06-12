#include "TerrainGenerator.hpp"
#include "shaders/terrain/terrain.h"
#include "shaders/resolve.h"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <etna/Sampler.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/DescriptorSet.hpp>

TerrainGenerator::TerrainGenerator()
{
  loadShaders();
  setupPipelines();
}

void TerrainGenerator::loadShaders()
{
  etna::create_program("perlin", {COMPLETE_RENDERER_SHADERS_ROOT "perlin.comp.spv"});
  etna::create_program("normal", {COMPLETE_RENDERER_SHADERS_ROOT "normal.comp.spv"});
  etna::create_program("lightgen", {COMPLETE_RENDERER_SHADERS_ROOT "lightgen.comp.spv"});
}

void TerrainGenerator::setupPipelines()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();

  perlinPipeline = pipelineManager.createComputePipeline("perlin", {});
  normalPipeline = pipelineManager.createComputePipeline("normal", {});
  lightgenPipeline = pipelineManager.createComputePipeline("lightgen", {});
}

TerrainGenerator::TerrainInfo TerrainGenerator::generate()
{
  auto& ctx = etna::get_context();
  TerrainInfo res;
  res.heightMap = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{terrain::heightMapSize.x, terrain::heightMapSize.y, 1},
    .name = "perlin_noise",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  res.normalMap = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{terrain::heightMapSize.x, terrain::heightMapSize.y, 1},
    .name = "normal_map",
    .format = vk::Format::eR8G8B8A8Snorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  res.lightList = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(resolve::PointLight) * 32,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "light_list",
  });

  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();

  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));

  createTerrainMap(cmdBuf, res);

  ETNA_CHECK_VK_RESULT(cmdBuf.end());

  cmdManager->submitAndWait(cmdBuf);

  return res;
}

void TerrainGenerator::createTerrainMap(vk::CommandBuffer cmd_buf, TerrainInfo& res)
{

  etna::set_state(
    cmd_buf,
    res.heightMap.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageRead,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);
  {
    auto info = etna::get_shader_program("perlin");

    auto binding0 = res.heightMap.genBinding(sampler.get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
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

    cmd_buf.dispatch(terrain::heightMapSize.x / 32, terrain::heightMapSize.y / 32, 1);
  }

  etna::set_state(
    cmd_buf,
    res.heightMap.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageRead,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::set_state(
    cmd_buf,
    res.normalMap.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  {
    auto info = etna::get_shader_program("normal");
    auto binding0 = res.heightMap.genBinding(sampler.get(), vk::ImageLayout::eGeneral, {});
    auto binding1 = res.normalMap.genBinding(sampler.get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
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

    cmd_buf.dispatch(terrain::heightMapSize.x / 32, terrain::heightMapSize.y / 32, 1);
  }

  {
    auto info = etna::get_shader_program("lightgen");
    auto binding0 = res.heightMap.genBinding(sampler.get(), vk::ImageLayout::eGeneral, {});
    auto binding1 = res.lightList.genBinding();

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, lightgenPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      lightgenPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);

    cmd_buf.dispatch(1, 1, 1);
  }

  etna::set_state(
    cmd_buf,
    res.heightMap.get(),
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    res.normalMap.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    res.lightList.get(),
    vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eVertexShader,
    vk::AccessFlagBits2::eUniformRead);
  etna::flush_barriers(cmd_buf);
}
