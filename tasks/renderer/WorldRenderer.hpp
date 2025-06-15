#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GpuSharedResource.hpp>
#include <glm/glm.hpp>

#include "StaticMeshRenderer.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"
#include "shaders/resolve.h"


class WorldRenderer
{
public:
  WorldRenderer() = default;

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines();

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image);

private:
  void createTerrainMap(vk::CommandBuffer cmd_buf);
  void renderTerrain(vk::CommandBuffer cmd_buf);
  void renderCube(vk::CommandBuffer cmd_buf);
  void tonemap(vk::CommandBuffer cmd_buf);
  void resolve(vk::CommandBuffer cmd_buf);

private:
  StaticMeshRenderer staticRenderer;

  etna::Image heightMap;
  etna::Image normalMap;
  etna::Buffer lightList;

  struct
  {
    etna::Image colorMetallic;
    etna::Image normalOcclusion;
    etna::Image emissiveRoughness; // reused as luminances
    etna::Image depth;
  } gBuffer;

  etna::Image tonemapDownscaledImage;
  etna::Buffer tonemapHist;
  etna::Sampler defaultSampler;

  glm::mat4x4 worldViewProj;
  glm::vec3 eye;

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

  float sunlightAngles[2] = {30.0f, 0.0f};
  struct
  {
    resolve::Sunlight sunlight = {
      glm::vec3(1.0 / 2, -glm::sqrt(3.0) / 2.0, 0), 0.0, glm::vec3(1, 1, 1), 0.05};

    glm::vec3 skyColor = glm::vec3(0, 181, 226) / 255.0f;
    float near;

    glm::mat4 mView;

    float far;
    float tanFov;
    float attenuationCoef = 1.0;
    float padding;
  } resolveUniformParams;

  struct
  {
    shader_bool forceLinear = false;
  } tonemapPushConstants;

  etna::Buffer resolveUniformParamsBuffer;

  glm::uvec2 resolution;
  glm::uvec2 downscaledRes;
};
