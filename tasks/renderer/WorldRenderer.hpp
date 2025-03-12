#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GpuSharedResource.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"
#include "shaders/resolve.h"


class WorldRenderer
{
public:
  WorldRenderer(const etna::GpuWorkCount& workCount);

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines();

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image);

private:
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);
  void createTerrainMap(vk::CommandBuffer cmd_buf);
  void renderTerrain(vk::CommandBuffer cmd_buf);
  void renderCube(vk::CommandBuffer cmd_buf);
  void tonemap(vk::CommandBuffer cmd_buf);
  void resolve(vk::CommandBuffer cmd_buf);

  bool shouldCull(glm::mat4 mModel, BoundingBox box);

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image heightMap;
  etna::Image normalMap;
  etna::Buffer lightList;

  struct
  {
    etna::Image color;
    etna::Image normal;
    etna::Image depthStencil;
  } gBuffer;

  etna::Image tonemapDownscaledImage;
  etna::Buffer tonemapHist;
  etna::GpuSharedResource<etna::Buffer> modelMatrices;
  etna::Sampler defaultSampler;

  glm::mat4x4 worldViewProj;
  glm::vec3 eye;

  etna::GraphicsPipeline staticMeshPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::ComputePipeline tonemapDownscalePipeline{};
  etna::ComputePipeline tonemapMinmaxPipeline{};
  etna::ComputePipeline tonemapEqualizePipeline{};
  etna::ComputePipeline tonemapHistPipeline{};
  etna::ComputePipeline tonemapCumsumPipeline{};
  etna::ComputePipeline tonemapPipeline{};
  etna::ComputePipeline resolvePipeline{};
  etna::GraphicsPipeline cubePipeline{};

  struct TerrainPushConst
  {
    glm::mat4 proj;
    glm::vec3 eye;
  };

  struct
  {
    resolve::Sunlight sunlight = {
      glm::vec3(1.0 / 2, -glm::sqrt(3.0) / 2.0, 0), 0.0, glm::vec3(1, 1, 1), 0.05};

    glm::vec3 skyColor = glm::vec3(135, 206, 235) / 255.0f;
    float lightExponent = 1.0f;

    glm::mat4 mView;

    float near;
    float far;
    float tanFov;
    float attenuationCoef = 0.005;
  } resolveUniformParams;

  etna::Buffer resolveUniformParamsBuffer;

  glm::uvec2 resolution;
  glm::uvec2 downscaledRes;
};
