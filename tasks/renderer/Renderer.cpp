#include "Renderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include "gui/ImGuiRenderer.hpp"
#include <imgui.h>

Renderer::Renderer(glm::uvec2 res)
  : resolution{res}
  , workCount{numFramesInFlight}
{
}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> instanceExtensions;

  for (auto ext : instance_extensions)
    instanceExtensions.push_back(ext);

  std::vector<const char*> deviceExtensions;

  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  etna::initialize(etna::InitParams{
    .applicationName = "model_bakery_renderer",
    .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
    .instanceExtensions = instanceExtensions,
    .deviceExtensions = deviceExtensions,
    .features =
      vk::PhysicalDeviceFeatures2{
        .features = {.tessellationShader = VK_TRUE, .multiDrawIndirect = VK_TRUE}},
    .physicalDeviceIndexOverride = {},
    .numFramesInFlight = numFramesInFlight,
    .generateBarriersAutomatically = false,
  });

  worldRenderer = std::make_unique<WorldRenderer>();
}

void Renderer::initFrameDelivery(vk::UniqueSurfaceKHR a_surface, ResolutionProvider res_provider)
{
  resolutionProvider = std::move(res_provider);

  auto& ctx = etna::get_context();

  commandManager = ctx.createPerFrameCmdMgr();

  window = ctx.createWindow(etna::Window::CreateInfo{
    .surface = std::move(a_surface),
  });

  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {resolution.x, resolution.y},
    .vsync = useVsync,
  });

  resolution = {w, h};

  worldRenderer->loadShaders();
  worldRenderer->setupPipelines();
  worldRenderer->allocateResources(resolution);
  guiRenderer = std::make_unique<ImGuiRenderer>(window->getCurrentFormat());
}

void Renderer::loadScene(std::filesystem::path path)
{
  worldRenderer->loadScene(path);
}

void Renderer::debugInput(const Keyboard& kb)
{
  worldRenderer->debugInput(kb);

  if (kb[KeyboardKey::kB] == ButtonState::Falling)
  {
    const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                                   " && cmake --build . --target model_bakery_renderer_shaders");
    if (retval != 0)
      spdlog::warn("Shader recompilation returned a non-zero return code!");
    else
    {
      ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
      etna::reload_shaders();
      spdlog::info("Successfully reloaded shaders!");
    }
  }
}

void Renderer::update(const FramePacket& packet)
{
  worldRenderer->update(packet);
}

void Renderer::drawFrame()
{
  ZoneScoped;

  {
    ZoneScopedN("drawGui");
    guiRenderer->nextFrame();
    ImGui::NewFrame();
    worldRenderer->drawGui();
    ImGui::Render();
  }

  auto currentCmdBuf = commandManager->acquireNext();

  etna::begin_frame();

  auto nextSwapchainImage = window->acquireNext();


  if (nextSwapchainImage)
  {
    auto [image, view, availableSem] = *nextSwapchainImage;
    // etna::get_context().getResourceTracker().setExternalTextureState(
    //   image,
    //   vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    //   vk::AccessFlagBits2::eNone,
    //   vk::ImageLayout::eUndefined);

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);

      worldRenderer->renderWorld(currentCmdBuf, image);

      etna::set_state(
        currentCmdBuf,
        image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      {
        ImDrawData* pDrawData = ImGui::GetDrawData();
        guiRenderer->render(
          currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, image, view, pDrawData);
      }

      etna::set_state(
        currentCmdBuf,
        image,
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(availableSem));

    const bool presented = window->present(std::move(renderingDone), view);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  if (!nextSwapchainImage && resolutionProvider() != glm::uvec2{0, 0})
  {
    auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }

  workCount.submit();
  etna::end_frame();
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
