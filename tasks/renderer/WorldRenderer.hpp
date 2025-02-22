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
  void tonemap(vk::CommandBuffer cmd_buf);
  void resolve(vk::CommandBuffer cmd_buf);

  bool shouldCull(glm::mat4 mModel, BoundingBox box);

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image heightMap;
  etna::Image normalMap;
  etna::Buffer lightList;

  struct {
    etna::Image color;
    etna::Image normal;
    etna::Image depthStencil;
  } gBuffer;

  etna::Buffer tonemapHist;
  etna::GpuSharedResource<etna::Buffer> modelMatrices;

  glm::mat4x4 worldViewProj;
  glm::mat4 worldView;
  glm::vec3 eye;
  float nearPlane;
  float farPlane;
  float tanFov;

  etna::GraphicsPipeline staticMeshPipeline{};
  etna::ComputePipeline perlinPipeline{};
  etna::ComputePipeline normalPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::ComputePipeline tonemapMinmaxPipeline{};
  etna::ComputePipeline tonemapEqualizePipeline{};
  etna::ComputePipeline tonemapHistPipeline{};
  etna::ComputePipeline tonemapCumsumPipeline{};
  etna::ComputePipeline tonemapPipeline{};
  etna::ComputePipeline resolvePipeline{};

  struct TerrainPushConst
  {
    glm::mat4 proj;
    glm::vec3 eye;
  };

  struct ResolvePushConstant{
    glm::mat4 mView;
    float near;
    float far;
    float tanFov;
  };

  etna::Sampler perlinSampler;

  glm::uvec2 resolution;
};
