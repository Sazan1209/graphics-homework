#ifndef RENDERINGPRIMITIVES_HPP
#define RENDERINGPRIMITIVES_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <etna/VertexInput.hpp>
#include <glm/glm.hpp>

struct BoundingBox
{
  std::array<float, 3> posMax;
  std::array<float, 3> posMin;
};

// struct Material
// {
// };

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t vertexCount;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;

  BoundingBox box;
  // Material
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

struct SceneData
{
  std::vector<unsigned char> vertIndBuffer;
  size_t indexOffset;
  std::span<const std::byte> getIndices()
  {
    return {
      reinterpret_cast<std::byte*>(vertIndBuffer.data() + indexOffset),
      vertIndBuffer.size() - indexOffset};
  };
  std::span<const std::byte> getVertexData()
  {
    return {reinterpret_cast<std::byte*>(vertIndBuffer.data()), indexOffset};
  };
  std::vector<SingleRE> singleRelems;
  std::vector<InstancedRE> groupedRelems;
  std::vector<REInstance> groupedRelemsInstances;
  std::vector<glm::mat4> transforms;

  etna::VertexByteStreamFormatDescription vertexDesc;
};

#endif // RENDERINGPRIMITIVES_HPP
