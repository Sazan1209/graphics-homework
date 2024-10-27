#pragma once

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/Sampler.hpp>
#include <etna/Window.hpp>
#include <etna/Buffer.hpp>

#include "shaders/UniformParams.h"
#include "wsi/OsWindowingManager.hpp"

class App
{
public:
  App();
  ~App();

  void run();

private:
  void uploadTexture();
  void compileShaders();
  void drawFrame();
  void drawProcTexture(vk::CommandBuffer& currentCmdBuf);
  void drawMainImage(vk::CommandBuffer& currentCmdBuf, vk::Image& backbuffer, vk::ImageView& backbufferView);
  void updateParams();
  void getInput();

  glm::vec2 mouse;
  float time;

  UniformParams params;

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  etna::GraphicsPipeline mainPipeline;
  etna::GraphicsPipeline procTexPipe;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
  etna::Sampler defaultSampler;
  etna::Image brassTexture;
  etna::Image procTexImage;
};
