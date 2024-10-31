#include "App.hpp"
#include "Alignstd430.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/Profiling.hpp>
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

void App::uploadTexture()
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
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
  });
  helper.uploadImage(
    *etna::get_context().createOneShotCmdMgr(),
    brassTexture,
    0,
    0,
    {data, static_cast<size_t>(x * y * 4)});
  stbi_image_free(data);
}

void App::compileShaders()
{
  etna::create_program(
    "toy",
    {INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv", INFLIGHT_FRAMES_SHADERS_ROOT "quad.vert.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();
  mainPipeline = pipelineManager.createGraphicsPipeline(
    "toy",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});

  etna::create_program(
    "proc",
    {INFLIGHT_FRAMES_SHADERS_ROOT "proc_texture.frag.spv",
     INFLIGHT_FRAMES_SHADERS_ROOT "quad.vert.spv"});

  procTexPipe = pipelineManager.createGraphicsPipeline(
    "proc",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});
}

App::App()
  : resolution{1280, 720}
  , useVsync{true}
  , workCount(2)
  , constants(workCount, std::in_place_t())
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    etna::initialize(etna::InitParams{
      .applicationName = "inflight_frames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = static_cast<uint32_t>(workCount.multiBufferingCount()),
    });
  }
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });
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
  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat, .name = "default_sampler"});

  uploadTexture();
  compileShaders();

  constants.iterate([&](auto& curr) {
    curr = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = alignedSize<UniformParams>,
      .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
      .name = "constants",
    });
    curr.map();
  });

  procTexImage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{256, 256, 1},
    .name = "procTexImage",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  });
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
    workCount.submit();
    FrameMark;
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::getInput()
{
  ZoneScoped;
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
  ZoneScoped;
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

void App::drawProcTexture(vk::CommandBuffer& currentCmdBuf)
{
  ZoneScoped;
  {
    ETNA_PROFILE_GPU(currentCmdBuf, procImage);
    etna::RenderTargetState state(
      currentCmdBuf, {{}, {256, 256}}, {{procTexImage.get(), procTexImage.getView({})}}, {});
    currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, procTexPipe.getVkPipeline());

    currentCmdBuf.pushConstants<float>(
      procTexPipe.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, {time});
    currentCmdBuf.draw(3, 1, 0, 0);

    etna::set_state(
      currentCmdBuf,
      procTexImage.get(),
      vk::PipelineStageFlagBits2::eFragmentShader,
      {vk::AccessFlagBits2::eShaderRead},
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(currentCmdBuf);
  }
}

void App::drawMainImage(
  vk::CommandBuffer& currentCmdBuf, vk::Image& backbuffer, vk::ImageView& backbufferView)
{
  ZoneScoped;
  {
    ETNA_PROFILE_GPU(currentCmdBuf, mainImage);
    etna::RenderTargetState state(
      currentCmdBuf, {{}, {resolution.x, resolution.y}}, {{backbuffer, backbufferView}}, {});

    auto info = etna::get_shader_program("toy");

    auto brassTextureBind =
      brassTexture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
    auto procTextureBind =
      procTexImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
    auto descriptorSet = etna::create_descriptor_set(
      info.getDescriptorLayoutId(0),
      currentCmdBuf,
      {etna::Binding{0, brassTextureBind},
       etna::Binding{1, procTextureBind},
       etna::Binding{2, constants.get().genBinding()}});
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

    std::memcpy(constants.get().data(), buf, sizeof(buf));

    currentCmdBuf.draw(3, 1, 0, 0);
    etna::flush_barriers(currentCmdBuf);
  }
}

void App::drawFrame()
{
  ZoneScoped;
  auto currentCmdBuf = commandManager->acquireNext();

  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, rendering);
      drawProcTexture(currentCmdBuf);
      drawMainImage(currentCmdBuf, backbuffer, backbufferView);

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
    ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
