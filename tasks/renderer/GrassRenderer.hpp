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
    renderPc.eyePos = eyePos_;
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
    glm::vec3 eyePos;
    float bend = .2;
    float tilt = glm::radians(15.0);
    float width = 0.02;
    float height = 0.6;
    float midCoef = 3.0 / 4.0;
    float time = 0.0;
    float jitterCoef = 0.1;
    float alignCoef = 0.7;
  } renderPc;

  struct GenPushConst{
    glm::mat4 mDfW;
    glm::vec3 eyePos;
    float maxJitter = 0.1;
    uint ring;
  } genPc;

  etna::GraphicsPipeline grassRenderPipeline;
  etna::ComputePipeline grassGenPipeline;
  etna::Buffer grassInstanceData;
  etna::Sampler sampler{
    etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "grass_sampler"}};
};

#endif
