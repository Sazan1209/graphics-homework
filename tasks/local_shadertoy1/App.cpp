#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>

static glm::mat3 yaw(float angle)
{
  float s = glm::sin(angle);
  float c = glm::cos(angle);
  return glm::mat3(c, s, 0, -s, c, 0, 0, 0, 1);
}
static glm::mat3 pitch(float angle)
{
  float c = cos(angle);
  float s = sin(angle);
  return glm::mat3(1, 0, 0, 0, c, s, 0, -s, c);
}

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS
    // window. Actually rendering anything to a screen is optional in Vulkan,
    // you can alternatively save rendered frames into files, send them over
    // network, etc. Instance extensions do not depend on the actual GPU, only
    // on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS
    // combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this
    // once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the
    // requested resolution and pick a different one. This, however, does not
    // occur on platforms we support. Still, it's better to follow the
    // "intended" path.
    resolution = {w, h};
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  auto& ctx = etna::get_context();

  // Alloc resources

  auto storageImage = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "storage",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
  });

  etna::create_program("comp", {LOCAL_SHADERTOY_SHADERS_ROOT "toy.comp.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();

  pipe = pipelineManager.createComputePipeline("comp", {});
  storage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "storage",
    .format = vk::Format::eR8G8B8A8Snorm,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc |
      vk::ImageUsageFlagBits::eSampled});

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    getInput();
    updateParams();
    drawFrame();
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::getInput()
{
  time = static_cast<float>(windowing.getTime());
  if (osWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
  {
    osWindow->captureMouse = !osWindow->captureMouse;
  }
  if (osWindow->captureMouse)
  {
    auto osMouse = osWindow->mouse;
    mouse += osMouse.capturedPosDelta;
    mouse = glm::clamp(mouse, glm::vec2(0, 0), glm::vec2(resolution));
  }
}

void App::updateParams()
{
  float pi2 = 3.1415926535897932384626433f * 2.0f;
  auto coef = mouse / glm::vec2(resolution);
  glm::mat3 rotation = yaw(coef.x * pi2) * pitch(-pi2 / 12.0f);
  glm::vec3 start = {0, 0.8, 0};
  float area = glm::sqrt(static_cast<float>(resolution.x * resolution.y));
  glm::vec3 plane_x = {0.5 / area, 0, 0};
  glm::vec3 plane_z = {0, 0, -0.5 / area};
  glm::mat3 mat = rotation * glm::mat3(plane_x, plane_z, start);
  glm::vec3 pos = {30.0 * sin(coef.x * pi2), -30.0 * cos(coef.x * pi2), 15.0};

  mat[2] += pos;
  params.camera = mat;
  assert(params.camera[0] == glm::vec4(mat[0], 0));
  params.cam_pos = glm::vec4{pos, 0};
  params.lightPos = {0, -5, 5, 0};
  params.time = time;
  params.resolution = glm::vec4{resolution, 0, 0};
}

void App::drawFrame()
{
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();

  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));

    {
      // Init
      etna::set_state(
        currentCmdBuf,
        storage.get(),
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderWrite,
        vk::ImageLayout::eGeneral,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      // Pipeline

      auto storageBinding = storage.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral);
      auto info = etna::get_shader_program("comp");

      auto descriptorSet = etna::create_descriptor_set(
        info.getDescriptorLayoutId(0), currentCmdBuf, {etna::Binding{0, storageBinding}});

      auto vkSet = descriptorSet.getVkSet();

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipe.getVkPipeline());
      currentCmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipe.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

      currentCmdBuf.pushConstants<UniformParams>(
        pipe.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, {params});

      etna::flush_barriers(currentCmdBuf);

      currentCmdBuf.dispatch((resolution.x - 1) / 32 + 1, (resolution.y - 1) / 32 + 1, 1);

      etna::flush_barriers(currentCmdBuf);
      // Change layout

      etna::set_state(
        currentCmdBuf,
        storage.get(),
        vk::PipelineStageFlagBits2::eBlit,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eGeneral,
        vk::ImageAspectFlagBits::eColor);
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eBlit,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      // Blit
      std::array<vk::Offset3D, 2> srcOffset = {
        vk::Offset3D{},
        vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1}};
      auto srdImageSubrecourceLayers = vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};

      // Destination info
      std::array<vk::Offset3D, 2> dstOffset = {
        vk::Offset3D{},
        vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1}};

      auto dstImageSubrecourceLayers = vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};
      // Create blit info
      auto imageBlit = vk::ImageBlit{
        .srcSubresource = srdImageSubrecourceLayers,
        .srcOffsets = srcOffset,
        .dstSubresource = dstImageSubrecourceLayers,
        .dstOffsets = dstOffset};

      currentCmdBuf.blitImage(
        storage.get(),
        vk::ImageLayout::eGeneral,
        backbuffer,
        vk::ImageLayout::eTransferDstOptimal,
        1,
        &imageBlit,
        vk::Filter::eLinear);
      etna::flush_barriers(currentCmdBuf);
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // This looks weird, but is correct. Ask about it later.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      // And of course flush the layout transition.
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be
    // executed by the GPU. Note that the GPU won't start executing our
    // commands before the semaphore is signalled, which will happen when the
    // OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU
    // tells the OS that it is done executing the command buffer via the
    // renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to
  // continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
