#include "GrassRenderer.hpp"
#include <imgui.h>
#include <tracy/Tracy.hpp>
#include <etna/Etna.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <ctime>

void GrassRenderer::loadShaders()
{
  ZoneScoped;
  etna::create_program(
    "render_grass",
    {COMPLETE_RENDERER_SHADERS_ROOT "grass.vert.spv",
     COMPLETE_RENDERER_SHADERS_ROOT "grass.frag.spv"});
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
}

void GrassRenderer::renderScene(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, render_grass);

  auto layout = grassRenderPipeline.getVkPipelineLayout();
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, grassRenderPipeline.getVkPipeline());
  cmd_buf.pushConstants<PushConst>(layout, vk::ShaderStageFlagBits::eVertex, 0, pc);
  cmd_buf.draw(15, 1, 0, 0);
}

void GrassRenderer::drawGui()
{
  if (ImGui::CollapsingHeader("Grass"))
  {
    ImGui::InputFloat("Bend", &pc.bend);
    ImGui::InputFloat2("Facing", &pc.facing[0]);
    ImGui::InputFloat("Height", &pc.height);
    ImGui::InputFloat("Middle point", &pc.midCoef);
    ImGui::InputFloat("Tilt", &pc.tilt);
    ImGui::InputFloat("Width", &pc.width);
    ImGui::InputFloat3("Pos", &pc.wPos[0]);
    ImGui::InputFloat("Jitter", &pc.jitterCoef);
  }
}
