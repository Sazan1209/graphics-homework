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
    float delta = time_ - renderPc.time;
    renderPc.mDfW = matrDfW_;
    renderPc.eyePos = eyePos_;
    renderPc.time = time_;
    float angleRad = glm::radians(360.0f * genPc.windAngle);
    renderPc.windPos += glm::vec2(glm::sin(angleRad), glm::cos(angleRad)) * delta * windSpeed;
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
    float bend = .1;

    glm::vec2 windPos = {0.0f, 0.0f};
    float tilt = 0.05f;
    float width = 0.03f;

    float height = 1.0f;
    float midCoef = 3.0f / 4.0f;
    float time = 0.0f;
    float jitterCoef = 0.1f;

    float alignCoef = 0.7f;
    float windChopSize = 8.0f;
    float windTiltStrength = 0.8f;
  } renderPc;

  struct GenPushConst
  {
    glm::mat4 mDfW;

    glm::vec3 eyePos;
    float maxRandomOffset = 1.0f;

    uint ring;
    float tilt;
    float height;
    float windAngle = 0;

    float windAlignForce = 0.5;
  } genPc;

  float windSpeed = 4.0;

  etna::GraphicsPipeline grassRenderPipeline;
  etna::ComputePipeline grassGenPipeline;
  etna::Buffer grassInstanceData;
  etna::Buffer grassDrawcallData;
  etna::Sampler sampler{
    etna::Sampler::CreateInfo{.filter = vk::Filter::eLinear, .name = "grass_sampler"}};
};

#endif
