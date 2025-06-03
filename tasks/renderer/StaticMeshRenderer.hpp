#ifndef STATICMESHRENDERER_HPP
#define STATICMESHRENDERER_HPP

#include <filesystem>
#include <glm/glm.hpp>
#include <etna/Etna.hpp>

#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/DescriptorSet.hpp>

class StaticMeshRenderer
{
public:
  ~StaticMeshRenderer();
  void loadScene(std::filesystem::path path);

  void loadShaders();
  void setupPipelines();

  void update(glm::mat4 matrVfW_) { matrVfW = matrVfW_; }

  void prepareForRender(vk::CommandBuffer cmd_buf);
  void renderScene(vk::CommandBuffer cmd_buf);

private:

  void createDescSet();

  etna::Buffer vertexData;
  etna::Buffer indexData;

  etna::Buffer relemMtWTransforms;

  // Contains drawcalls, culling info and render info combined in one
  size_t singleRelemCount;
  etna::Buffer singleRelemData;

  size_t groupRelemCount;
  size_t groupRelemInstanceCount;
  etna::Buffer drawCallsGRE;
  etna::Buffer cullingInfoGRE;
  etna::Buffer renderInfoGRE;

  etna::GraphicsPipeline singleRenderPipeline;
  etna::GraphicsPipeline groupRenderPipeline;
  etna::ComputePipeline groupResetPipeline;
  etna::ComputePipeline groupCullPipeline;
  etna::ComputePipeline singleCullPipeline;

  etna::VertexByteStreamFormatDescription vertexDesc;

  std::vector<etna::Image> textures;
  vk::DescriptorSet textureSet;
  vk::DescriptorPool textureSetPool;
  vk::DescriptorSetLayout textureSetLayout;

  glm::mat4 matrVfW{};

  constexpr static size_t MAX_DESCRIPTOR_NUM = 128;
};

#endif // STATICMESHRENDERER_HPP
