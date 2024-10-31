#ifndef BAKER_HPP
#define BAKER_HPP

#include <filesystem>

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <etna/Buffer.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/VertexInput.hpp>

#include <stb_image.h>

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t vertexCount;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;
  std::array<double, 3> posMax;
  std::array<double, 3> posMin;

  // Material* material;
};

struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

class Baker
{
public:
  Baker() = default;

  void selectScene(std::filesystem::path path);
  void bakeScene(std::filesystem::path path);

private:
  std::optional<tinygltf::Model> loadModel(std::filesystem::path path);

  struct Vertex
  {
    // First 3 floats are position, 4th float is a packed normal
    glm::vec4 positionAndNormal;
    // First 2 floats are tex coords, 3rd is a packed tangent, 4th is padding
    glm::vec4 texCoordAndTangentAndPadding;
  };

  static_assert(sizeof(Vertex) == sizeof(float) * 8);

  struct ProcessedMeshes
  {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<RenderElement> relems;
    std::vector<Mesh> meshes;
  };
  ProcessedMeshes processMeshes(const tinygltf::Model& model) const;
  void ProcessAttribute(
    const tinygltf::Model& model, int accesor_ind, std::span<Vertex>& vertices, auto setter) const;

private:
  tinygltf::TinyGLTF loader;
};

#endif // BAKER_HPP
