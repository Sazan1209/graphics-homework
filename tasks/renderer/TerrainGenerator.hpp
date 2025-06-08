#ifndef TERRAINGENERATOR_HPP
#define TERRAINGENERATOR_HPP

#include <etna/Image.hpp>
#include <etna/Buffer.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Sampler.hpp>
#include <glm/glm.hpp>

class TerrainGenerator
{
public:
  struct TerrainInfo
  {
    etna::Image heightMap;
    etna::Image normalMap;
    etna::Buffer lightList;
  };
  TerrainGenerator();
  TerrainInfo generate();

private:
  etna::ComputePipeline perlinPipeline{};
  etna::ComputePipeline normalPipeline{};
  etna::ComputePipeline lightgenPipeline{};

  void createTerrainMap(vk::CommandBuffer cmd_buf, TerrainInfo& info);
  void loadShaders();
  void setupPipelines();
  etna::Sampler sampler{
    etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "perlinSampler"}};
};

#endif // TERRAINGENERATOR_HPP
