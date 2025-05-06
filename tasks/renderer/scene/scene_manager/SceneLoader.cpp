#include "SceneLoader.hpp"

#include <limits>
#include <stack>

#include <spdlog/spdlog.h>
#include <fmt/std.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/OneShotCmdMgr.hpp>


std::optional<tinygltf::Model> SceneLoader::loadModel(std::filesystem::path path)
{
  tinygltf::Model model;

  std::string error;
  std::string warning;
  bool success = false;

  auto ext = path.extension();
  if (ext == ".gltf")
    success = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
  else if (ext == ".glb")
    success = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
  else
  {
    spdlog::error("glTF: Unknown glTF file extension: '{}'. Expected .gltf or .glb.", ext);
    return std::nullopt;
  }

  if (!success)
  {
    spdlog::error("glTF: Failed to load model!");
    if (!error.empty())
      spdlog::error("glTF: {}", error);
    return std::nullopt;
  }

  if (!warning.empty())
    spdlog::warn("glTF: {}", warning);

  if (
    !model.extensions.empty() || !model.extensionsRequired.empty() || !model.extensionsUsed.empty())
    spdlog::warn("glTF: No glTF extensions are currently implemented!");

  return model;
}

std::optional<SceneData> SceneLoader::selectScene(std::filesystem::path path)
{
  auto maybeModel = loadModel(path);
  if (!maybeModel.has_value())
    return std::nullopt;

  auto model = std::move(*maybeModel);

  // Resolve node transforms
  auto [transforms, instMeshes] = processInstances(model);
  // Unpack vertex data and relems
  auto [vertIndBuffer, indexOffset, relems, meshs] = processMeshes(model);

  // Group repeating relems and separete the non-repeating ones
  auto [groupedRelemsInstances, groupedRelems, singleRelems] =
    processGroups(instMeshes, meshs, relems);
  auto vertexDesc = getVertexFormatDescription();

  return SceneData{
    .vertIndBuffer = std::move(vertIndBuffer),
    .indexOffset = indexOffset,
    .singleRelems = std::move(singleRelems),
    .groupedRelems = std::move(groupedRelems),
    .groupedRelemsInstances = std::move(groupedRelemsInstances),
    .transforms = std::move(transforms),
    .vertexDesc = std::move(vertexDesc)};
}

SceneLoader::ProcessedInstances SceneLoader::processInstances(const tinygltf::Model& model) const
{
  std::vector nodeTransforms(model.nodes.size(), glm::identity<glm::mat4x4>());

  for (std::size_t nodeIdx = 0; nodeIdx < model.nodes.size(); ++nodeIdx)
  {
    const auto& node = model.nodes[nodeIdx];
    auto& transform = nodeTransforms[nodeIdx];

    if (!node.matrix.empty())
    {
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          transform[i][j] = static_cast<float>(node.matrix[4 * i + j]);
    }
    else
    {
      if (!node.scale.empty())
        transform = scale(
          transform,
          glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])));

      if (!node.rotation.empty())
        transform *= mat4_cast(glm::quat(
          static_cast<float>(node.rotation[3]),
          static_cast<float>(node.rotation[0]),
          static_cast<float>(node.rotation[1]),
          static_cast<float>(node.rotation[2])));

      if (!node.translation.empty())
        transform = translate(
          transform,
          glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])));
    }
  }
  std::stack<std::size_t> vertices;
  for (auto vert : model.scenes[model.defaultScene].nodes)
    vertices.push(vert);

  while (!vertices.empty())
  {
    auto vert = vertices.top();
    vertices.pop();

    for (auto child : model.nodes[vert].children)
    {
      nodeTransforms[child] = nodeTransforms[vert] * nodeTransforms[child];
      vertices.push(child);
    }
  }

  ProcessedInstances result;

  // Don't overallocate matrices, they are pretty chonky.
  {
    std::size_t totalNodesWithMeshes = 0;
    for (const auto& node : model.nodes)
      if (node.mesh >= 0)
      {
        ++totalNodesWithMeshes;
      }
    result.matrices.reserve(totalNodesWithMeshes);
    result.meshes.reserve(totalNodesWithMeshes);
  }

  for (std::size_t i = 0; i < model.nodes.size(); ++i)
    if (model.nodes[i].mesh >= 0)
    {
      result.matrices.push_back(nodeTransforms[i]);
      result.meshes.push_back(model.nodes[i].mesh);
    }

  return result;
}

SceneLoader::ProcessedMeshes SceneLoader::processMeshes(const tinygltf::Model& model) const
{

  ProcessedMeshes result;

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
  }

  result.meshes.reserve(model.meshes.size());

  for (const auto& mesh : model.meshes)
  {
    {
      Mesh curr = Mesh{
        .firstRelem = static_cast<std::uint32_t>(result.relems.size()),
        .relemCount = static_cast<std::uint32_t>(mesh.primitives.size()),
      };
      result.meshes.push_back(curr);
    }

    for (const auto& prim : mesh.primitives)
    {
      auto& ind_accessor = model.accessors[prim.indices];
      auto& pos_accessor = model.accessors[prim.attributes.at("POSITION")];
      BoundingBox box;

      std::copy(pos_accessor.maxValues.begin(), pos_accessor.maxValues.end(), box.posMax.begin());
      std::copy(pos_accessor.minValues.begin(), pos_accessor.minValues.end(), box.posMin.begin());

      result.relems.push_back(RenderElement{
        .vertexOffset = static_cast<std::uint32_t>(pos_accessor.byteOffset / sizeof(Vertex)),
        .vertexCount = static_cast<std::uint32_t>(pos_accessor.count),
        .indexOffset = static_cast<std::uint32_t>(ind_accessor.byteOffset / sizeof(uint32_t)),
        .indexCount = static_cast<std::uint32_t>(ind_accessor.count),
        .box = box,
      });
    }
  }

  {
    result.indexOffset = model.bufferViews[0].byteLength;
    result.vertIndBuffer = std::move(model.buffers[0].data);
  }

  return result;
}

SceneLoader::ProcessedGroups SceneLoader::processGroups(
  const std::vector<uint32_t>& instMeshes,
  const std::vector<Mesh>& meshes,
  const std::vector<RenderElement>& relems)
{
  if (relems.empty())
  {
    spdlog::warn("processGroups called with empty relem vector");
    return {};
  }
  auto forEachRelem = [&](const auto& func) {
    for (size_t i = 0; i < instMeshes.size(); ++i)
    {
      size_t meshInd = instMeshes[i];
      auto& currMesh = meshes[meshInd];
      for (size_t relemInd = 0; relemInd < currMesh.relemCount; ++relemInd)
      {
        func(relemInd + currMesh.firstRelem, i);
      }
    }
  };

  std::vector<size_t> relemUseCounts(relems.size(), 0);
  forEachRelem([&](size_t relemInd, size_t) { relemUseCounts[relemInd] += 1; });

  // Filter single relems
  size_t singleRelemCount = 0;
  for (auto curr : relemUseCounts)
  {
    if (curr == 1)
    {
      ++singleRelemCount;
    }
  }
  std::vector<bool> isSingle(relems.size());
  std::vector<SingleRE> singleRelems;
  singleRelems.reserve(singleRelemCount);
  forEachRelem([&](size_t relemInd, size_t meshInd) {
    if (relemUseCounts[relemInd] == 1)
    {
      isSingle[relemInd] = true;
      relemUseCounts[relemInd] = 0;
      SingleRE curr = {
        .element = relems[relemInd],
        .matrixPos = static_cast<uint32_t>(meshInd),
      };
      singleRelems.push_back(curr);
    }
  });

  // Group remaining relems
  std::vector<InstancedRE> relemGroups;
  std::vector<uint32_t> groupInds(relems.size());
  relemGroups.reserve(relems.size() - singleRelemCount);
  if (relemUseCounts[0] != 0)
  {
    relemGroups.push_back(InstancedRE{
      .element = relems[0],
      .firstInstance = 0,
      .instanceCount = static_cast<uint32_t>(relemUseCounts[0]),
    });
    groupInds[0] = 0;
  }
  for (size_t i = 1; i < relemUseCounts.size(); ++i)
  {
    if (relemUseCounts[i] != 0)
    {
      groupInds[i] = relemGroups.size();
      relemGroups.push_back(InstancedRE{
        .element = relems[i],
        .firstInstance = static_cast<uint32_t>(relemUseCounts[i - 1]),
        .instanceCount = static_cast<uint32_t>(relemUseCounts[i]),
      });
    }
    relemUseCounts[i] += relemUseCounts[i - 1];
  }
  std::vector<REInstance> instances(relemUseCounts.back());
  forEachRelem([&](size_t relemInd, size_t meshInd) {
    if (isSingle[relemInd])
    {
      return; // continue
    }
    REInstance curr = {
      .matrixPos = static_cast<uint32_t>(meshInd),
      .groupPos = groupInds[relemInd],
    };
    instances[--relemUseCounts[relemInd]] = curr;
  });
  return ProcessedGroups{
    .groupedRelemsInstances = std::move(instances),
    .groupedRelems = std::move(relemGroups),
    .singleRelems = std::move(singleRelems),
  };
}

etna::VertexByteStreamFormatDescription SceneLoader::getVertexFormatDescription() const
{
  return etna::VertexByteStreamFormatDescription{
    .stride = sizeof(Vertex),
    .attributes = {
      etna::VertexByteStreamFormatDescription::Attribute{
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 0,
      },
      etna::VertexByteStreamFormatDescription::Attribute{
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = sizeof(glm::vec4),
      },
    }};
}
