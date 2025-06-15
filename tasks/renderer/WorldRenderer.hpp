#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GpuSharedResource.hpp>
#include <glm/glm.hpp>

#include "StaticMeshRenderer.hpp"
#include "GrassRenderer.hpp"
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
  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  void createTerrainMap(vk::CommandBuffer cmd_buf);
  void renderTerrain(vk::CommandBuffer cmd_buf);
  void renderCube(vk::CommandBuffer cmd_buf);
  void tonemap(vk::CommandBuffer cmd_buf);
  void resolve(vk::CommandBuffer cmd_buf);
  void fxaaPresent(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
  void present(vk::CommandBuffer cmd_buf, vk::Image target_image);

private:
  StaticMeshRenderer staticRenderer;
  GrassRenderer grassRenderer;

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
  etna::GraphicsPipeline fxaaPipeline{};


  struct TerrainPushConst
  {
    glm::mat4 proj;
    glm::vec3 eye;
  };

  float sunlightAngles[2] = {30.0f, 0.0f};
  struct
  {
    resolve::Sunlight sunlight = {
      glm::vec3(1.0 / 2, -glm::sqrt(3.0) / 2.0, 0), 1.0, glm::vec3(1, 1, 1), 0.05};

    glm::vec3 skyColor = glm::vec3(0, 181, 226) / 255.0f;
    float near;

    glm::mat4 mView;

    float far;
    float tanFov;
    float attenuationCoef = 0.05;
    float padding;
  } resolveUniformParams;

  struct
  {
    shader_bool forceLinear = false;
    shader_bool useFxaa;
  } tonemapPushConstants;

  etna::Buffer resolveUniformParamsBuffer;

  bool useFxaa = true;
  struct {
    glm::vec2 rcpFrame;
    // Choose the amount of sub-pixel aliasing removal.
    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    float subpix = 0.75;
    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality
    //   0.063 - overkill (slower)
    float edgeThreshold = 0.166;
    // Trims the algorithm from processing darks.
    //   0.0833 - upper limit (default, the start of visible unfiltered edges)
    //   0.0625 - high quality (faster)
    //   0.0312 - visible limit (slower)
    // Special notes when using FXAA_GREEN_AS_LUMA,
    //   Likely want to set this to zero.
    //   As colors that are mostly not-green
    //   will appear very dark in the green channel!
    //   Tune by looking at mostly non-green content,
    //   then start at zero and increase until aliasing is a problem.
    float edgeThresholdMin = 0.0833;
  } fxaaPushConstants;

  glm::uvec2 resolution;
  glm::uvec2 downscaledRes;
};
