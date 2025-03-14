#ifndef RENDERINGPRIMITIVES_HPP
#define RENDERINGPRIMITIVES_HPP

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
  std::uint32_t relemPos;
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

struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

#endif // RENDERINGPRIMITIVES_HPP
