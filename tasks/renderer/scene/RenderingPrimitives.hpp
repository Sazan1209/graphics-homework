#ifndef RENDERINGPRIMITIVES_HPP
#define RENDERINGPRIMITIVES_HPP

struct BoundingBox
{
  std::array<float, 3> posMax;
  std::array<float, 3> posMin;
};

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t vertexCount;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;

  BoundingBox box;
};

struct REInstance{
  std::uint32_t matrixPos;
  std::uint32_t materialPos;
};

struct REGroup{
  RenderElement element;
  std::uint32_t firstInstance;
  std::uint32_t instanceCount;
};

struct Material{

};

struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

#endif // RENDERINGPRIMITIVES_HPP
