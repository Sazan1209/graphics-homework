#include "GrassRenderer.hpp"
#include <imgui.h>
#include <tracy/Tracy.hpp>
#include <etna/Etna.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <ctime>
#include "shaders/grass/grass.h"

void GrassRenderer::loadShaders()
{
  ZoneScoped;
  etna::create_program("generate_grass", {COMPLETE_RENDERER_SHADERS_ROOT "grassGen.comp.spv"});
  etna::create_program(
    "render_grass",
    {COMPLETE_RENDERER_SHADERS_ROOT "grass.vert.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "grass.frag.spv"});
}

void GrassRenderer::allocateResources()
{
  ZoneScoped;
  grassInstanceData = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = grass::drawnGrassCount.x * grass::drawnGrassCount.y * sizeof(grass::GrassInstanceData),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_COPY,
    .name = "grass_instance_data"});
}

void GrassRenderer::setupPipelines()
{
  ZoneScoped;
  auto& pipelineManager = etna::get_context().getPipelineManager();
  grassRenderPipeline =
    pipelineManager.createGraphicsPipeline(
      "render_grass",
      etna::GraphicsPipeline::CreateInfo{
        .inputAssemblyConfig =
          {
            .topology = vk::PrimitiveTopology::eTriangleStrip,
          },
        .rasterizationConfig =
          vk::PipelineRasterizationStateCreateInfo{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
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
              {vk::Format::eA8B8G8R8SrgbPack32,
               vk::Format::eA8B8G8R8SnormPack32,
               vk::Format::eA8B8G8R8SrgbPack32},
            .depthAttachmentFormat = vk::Format::eD32Sfloat,
          },
      });
  grassGenPipeline = pipelineManager.createComputePipeline("generate_grass", {});
}

void GrassRenderer::prepareForRender(vk::CommandBuffer cmd_buf, etna::Image& heightMap)
{
  etna::set_state(
    cmd_buf,
    grassInstanceData.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageWrite);
  etna::flush_barriers(cmd_buf);
  {
    auto info = etna::get_shader_program("generate_grass");
    auto binding0 =
      heightMap.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {});
    auto binding1 = grassInstanceData.genBinding();

    auto set = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });

    vk::DescriptorSet vkSet = set.getVkSet();
    auto& pipeline = grassGenPipeline;
    auto layout = grassGenPipeline.getVkPipelineLayout();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, {vkSet}, {});
    cmd_buf.pushConstants<GenPushConst>(layout, vk::ShaderStageFlagBits::eCompute, 0, genPc);

    cmd_buf.dispatch(grass::drawnGrassCount.x, grass::drawnGrassCount.y, 1);
  }
  etna::set_state(
    cmd_buf,
    grassInstanceData.get(),
    vk::PipelineStageFlagBits2::eVertexShader,
    vk::AccessFlagBits2::eShaderStorageRead);
}

void GrassRenderer::renderScene(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, render_grass);
  auto info = etna::get_shader_program("render_grass");
  auto binding0 = grassInstanceData.genBinding();

  auto set = etna::create_descriptor_set(
    info.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, binding0},
    });

  vk::DescriptorSet vkSet = set.getVkSet();
  auto layout = grassRenderPipeline.getVkPipelineLayout();
  auto& pipeline = grassRenderPipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 0, {vkSet}, {});

  cmd_buf.pushConstants<RenderPushConst>(layout, vk::ShaderStageFlagBits::eVertex, 0, renderPc);
  cmd_buf.draw(15, grass::drawnGrassCount.x * grass::drawnGrassCount.y, 0, 0);
}

void GrassRenderer::drawGui()
{
  if (ImGui::CollapsingHeader("Grass"))
  {
    ImGui::InputFloat("Bend", &renderPc.bend);
    ImGui::InputFloat("Height", &renderPc.height);
    ImGui::InputFloat("Middle point", &renderPc.midCoef);
    ImGui::InputFloat("Tilt", &renderPc.tilt);
    ImGui::InputFloat("Width", &renderPc.width);
    ImGui::InputFloat("Jitter", &renderPc.jitterCoef);
    ImGui::InputFloat("Force align", &renderPc.alignCoef);
  }
  if (ImGui::CollapsingHeader("Grass Gen"))
  {
    ImGui::InputFloat("Grass position jitter", &genPc.maxJitter);
  }
}
