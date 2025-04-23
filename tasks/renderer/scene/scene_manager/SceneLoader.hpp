#pragma once

#include <filesystem>
#include <optional>
#include <cinttypes>

#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include "RenderingPrimitives.hpp"

class SceneLoader
{
public:
  std::optional<SceneData> selectScene(std::filesystem::path path);

private:
  std::optional<tinygltf::Model> loadModel(std::filesystem::path path);

  struct ProcessedInstances
  {
    std::vector<glm::mat4x4> matrices;
    std::vector<std::uint32_t> meshes;
  };

  ProcessedInstances processInstances(const tinygltf::Model& model) const;

  struct Mesh
  {
    std::uint32_t firstRelem;
    std::uint32_t relemCount;
  };

  struct ProcessedMeshes
  {
    std::vector<unsigned char> vertIndBuffer;
    std::span<const Vertex> vertices;
    std::span<const std::uint32_t> indices;
    std::vector<RenderElement> relems;
    std::vector<Mesh> meshes;
  };

  ProcessedMeshes processMeshes(const tinygltf::Model& model) const;

  struct ProcessedGroups
  {
    std::vector<REInstance> groupedRelemsInstances;
    std::vector<InstancedRE> groupedRelems;
    std::vector<SingleRE> singleRelems;
  };

  ProcessedGroups processGroups(
    const std::vector<uint32_t>& instMeshes,
    const std::vector<Mesh>& meshes,
    const std::vector<RenderElement>& relems);

  etna::VertexByteStreamFormatDescription getVertexFormatDescription() const;

private:
  tinygltf::TinyGLTF loader;
};
