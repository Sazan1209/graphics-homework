#ifndef RENDERINGPRIMITIVES_HPP
#define RENDERINGPRIMITIVES_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <etna/VertexInput.hpp>
#include <glm/glm.hpp>
#include <etna/Buffer.hpp>
#include <etna/Image.hpp>

struct BoundingBox
{
  std::array<float, 3> posMax;
  std::array<float, 3> posMin;
};

struct Material
{
  static constexpr uint32_t MISSING_INDEX = -1;

  uint32_t albedoIndex = MISSING_INDEX;
  glm::vec4 albedoFactor = {1.0, 1.0, 1.0, 1.0};

  uint32_t metallicRoughnessIndex = MISSING_INDEX;
  float metallicFactor = 1.0;
  float roughnessFactor = 1.0;

  uint32_t normalIndex = MISSING_INDEX;
  float normalScale = 1.0;

  uint32_t occlusionIndex = MISSING_INDEX;
  float occlusionStrength;

  uint32_t emissiveIndex = MISSING_INDEX;
  glm::vec3 emissiveFactor = {0, 0, 0};

};

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t vertexCount;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;

  BoundingBox box;
  Material material = {};
};

struct REInstance
{
  std::uint32_t matrixPos;
  std::uint32_t groupPos;
};

struct InstancedRE
{
  RenderElement element;
  std::uint32_t firstInstance;
  std::uint32_t instanceCount;
};

struct SingleRE
{
  RenderElement element;
  std::uint32_t matrixPos;
};

struct Vertex
{
  // First 3 floats are position, 4th float is a packed normal
  glm::vec4 positionAndNormal;
  // First 2 floats are tex coords, 3rd is a packed tangent, 4th is padding
  glm::vec4 texCoordAndTangentAndPadding;
};

static_assert(sizeof(Vertex) == sizeof(float) * 8);

struct Texture{
  etna::Image image;
};

struct SceneData
{
  // GPU-only data
  etna::Buffer vertexData;
  etna::Buffer indexData;
  std::vector<etna::Image> textures;

  std::vector<SingleRE> singleRelems;
  std::vector<InstancedRE> groupedRelems;
  std::vector<REInstance> groupedRelemsInstances;
  std::vector<glm::mat4> transforms;

  etna::VertexByteStreamFormatDescription vertexDesc;
};

#endif // RENDERINGPRIMITIVES_HPP
