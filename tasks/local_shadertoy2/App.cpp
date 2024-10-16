#include "App.hpp"
#include "Alignstd430.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include "stb_image.h"

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

  //
  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    resolution = {w, h};
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();

  // Upload texture
  {
    int x, y, n;
    auto maybeData = stbi_load(GRAPHICS_COURSE_RESOURCES_ROOT "/textures/brass.png", &x, &y, &n, 4);
    assert(maybeData != nullptr && "couldn't read brass texture\n");
    auto data = reinterpret_cast<std::byte*>(maybeData);

    etna::BlockingTransferHelper helper({.stagingSize = static_cast<vk::DeviceSize>(x * y * 4)});
    brassTexture = etna::get_context().createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1},
      .name = "brassTexture",
      .format = vk::Format::eR8G8B8A8Unorm,
      .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferDst,
    });
    helper.uploadImage(
      *etna::get_context().createOneShotCmdMgr(),
      brassTexture,
      0,
      0,
      {data, static_cast<size_t>(x * y * 4)});
    stbi_image_free(data);
  }

  // Alloc buffer for proc texture

  procTexBuffer = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{256, 256, 1},
    .name = "procTexBuffer",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  });

  // Compile pipelines
  etna::create_program(
    "toy",
    {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv", LOCAL_SHADERTOY2_SHADERS_ROOT "quad.vert.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();
  mainPipeline = pipelineManager.createGraphicsPipeline(
    "toy",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});

  etna::create_program(
    "proc",
    {LOCAL_SHADERTOY2_SHADERS_ROOT "proc_texture.frag.spv",
     LOCAL_SHADERTOY2_SHADERS_ROOT "quad.vert.spv"});

  procTexPipe = pipelineManager.createGraphicsPipeline(
    "proc",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat, .name = "default_sampler"});
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
  constexpr float pi = 3.1415926535897932384626433f;
  constexpr float fov = pi * 4.0f / 12.0f; // 75 degrees
  constexpr float dist = 0.8f;

  glm::vec3 depth = glm::vec3{0, 1, 0} * dist;
  float ratio = static_cast<float>(resolution.y) / static_cast<float>(resolution.x);
  glm::vec3 plane_x = glm::vec3{1, 0, 0} * std::tan(fov / 2.0f) * 2.0f;
  glm::vec3 plane_z = glm::vec3{0, 0, -1} * std::tan(fov / 2.0f) * ratio * 2.0f;
  glm::vec3 start = depth - 0.5f * (plane_x + plane_z);

  auto coef = mouse / glm::vec2(resolution);
  float angle = coef.x * pi * 2.0f;
  glm::mat3 rotation = yaw(angle) * pitch(-pi / 6.0f);

  glm::mat3 mat = rotation * glm::mat3(plane_x, plane_z, start);
  glm::vec3 pos = {10.0f * sin(angle), -10.0f * cos(angle), 5.0f};

  mat[2] += pos;
  params.camera = mat;
  params.cam_pos = glm::vec4{pos, 0};
  params.lightPos = {0, -5, 5};
  params.time = time;
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
      {

        etna::RenderTargetState state(
          currentCmdBuf, {{}, {256, 256}}, {{procTexBuffer.get(), procTexBuffer.getView({})}}, {});
        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, procTexPipe.getVkPipeline());

        currentCmdBuf.pushConstants<float>(
          procTexPipe.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, {time});
        currentCmdBuf.draw(3, 1, 0, 0);
      }
      etna::flush_barriers(currentCmdBuf);

      etna::set_state(
        currentCmdBuf,
        procTexBuffer.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        {},
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);


      {

        etna::RenderTargetState state(
          currentCmdBuf, {{}, {resolution.x, resolution.y}}, {{backbuffer, backbufferView}}, {});

        auto info = etna::get_shader_program("toy");

        auto brassTextureBind =
          brassTexture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
        auto procTextureBind =
          procTexBuffer.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
        auto descriptorSet = etna::create_descriptor_set(
          info.getDescriptorLayoutId(0),
          currentCmdBuf,
          {etna::Binding{0, brassTextureBind}, etna::Binding{1, procTextureBind}});
        auto vkSet = descriptorSet.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          mainPipeline.getVkPipelineLayout(),
          0,
          1,
          &vkSet,
          0,
          nullptr);

        AlignedBuffer<UniformParams> buf{};
        memcpy_aligned_std430(buf, params);

        currentCmdBuf.pushConstants<AlignedBuffer<UniformParams>>(
          mainPipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, {buf});

        currentCmdBuf.draw(3, 1, 0, 0);
      }
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
