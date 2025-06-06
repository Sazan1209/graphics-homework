#ifndef GRASSRENDERER_HPP
#define GRASSRENDERER_HPP

#include <glm/glm.hpp>
#include <etna/GraphicsPipeline.hpp>


class GrassRenderer
{
public:
  void loadShaders();
  void setupPipelines();

  void update(glm::mat4 matrDfW_, float time_)
  {
    pc.mDfW = matrDfW_;
    pc.time = time_;
  }

  void renderScene(vk::CommandBuffer cmd_buf);
  void drawGui();

private:
  struct PushConst
  {
    glm::mat4 mDfW;
    glm::vec3 wPos = {0, 0, -1.0};
    float bend = .5;
    glm::vec2 facing = {1, 0};
    float tilt = glm::radians(30.0);
    float width = .1;
    float height = 1.0;
    float midCoef = 3.0 / 4.0;
    float time = 0.0;
    float jitterCoef = 0.5;
  };
  PushConst pc;
  etna::GraphicsPipeline grassRenderPipeline;
};

#endif
