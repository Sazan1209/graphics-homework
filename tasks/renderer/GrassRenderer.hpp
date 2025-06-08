#ifndef GRASSRENDERER_HPP
#define GRASSRENDERER_HPP

#include <glm/glm.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Buffer.hpp>
#include <etna/Sampler.hpp>
#include <etna/Image.hpp>

class GrassRenderer
{
public:
  void loadShaders();
  void allocateResources();
  void prepareForRender(vk::CommandBuffer cmd_buf, etna::Image& heightMap);
  void setupPipelines();

  void update(glm::mat4 matrDfW_, float time_, glm::vec3 eyePos_)
  {
    renderPc.mDfW = matrDfW_;
    renderPc.time = time_;
    genPc.mDfW = matrDfW_;
    genPc.eyePos = eyePos_;
  }

  void renderScene(vk::CommandBuffer cmd_buf);
  void drawGui();

private:
  struct RenderPushConst
  {
    glm::mat4 mDfW;
    glm::vec2 facing = {1.0, 0};
    float bend = .5;
    float tilt = glm::radians(30.0);
    float width = .01;
    float height = .05;
    float midCoef = 3.0 / 4.0;
    float time = 0.0;
    float jitterCoef = 0.5;
  } renderPc;

  struct GenPushConst{
    glm::mat4 mDfW;
    glm::vec3 eyePos;
  } genPc;

  etna::GraphicsPipeline grassRenderPipeline;
  etna::ComputePipeline grassGenPipeline;
  etna::Buffer grassInstanceData;
  etna::Sampler sampler{
    etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "grass_sampler"}};
};

#endif
