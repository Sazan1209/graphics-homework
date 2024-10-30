#include "baker.hpp"

#include <stack>

#include <spdlog/spdlog.h>
#include <fmt/std.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <fstream>


std::optional<tinygltf::Model> Baker::loadModel(std::filesystem::path path)
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

static std::uint32_t encode_normal(glm::vec4 normal)
{

  const int8_t x = static_cast<int8_t>(round(normal.x * 127.0));
  const int8_t y = static_cast<int8_t>(round(normal.y * 127.0));
  const int8_t z = static_cast<int8_t>(round(normal.z * 127.0));
  const int8_t w = static_cast<int8_t>(round(normal.w * 127.0));

  int8_t arr[4] = {x, y, z, w};

  return std::bit_cast<uint32_t>(arr);
}

static std::uint32_t encode_normal(glm::vec3 normal)
{
  return encode_normal(glm::vec4{normal, 0});
}

void Baker::ProcessAttribute(
  const tinygltf::Model& model, int accessor_ind, std::span<Vertex>& vertices, auto setter) const
{
  auto& accessor = model.accessors[accessor_ind];
  auto& bufView = model.bufferViews[accessor.bufferView];
  auto ptr = reinterpret_cast<const std::byte*>(model.buffers[bufView.buffer].data.data()) +
    bufView.byteOffset + accessor.byteOffset;
  auto stride = bufView.byteStride != 0
    ? bufView.byteStride
    : tinygltf::GetComponentSizeInBytes(accessor.componentType) *
      tinygltf::GetNumComponentsInType(accessor.type);
  for (auto& vtx : vertices)
  {
    setter(vtx, ptr);
    ptr += stride;
  }
}

static void updateMinMax(RenderElement& relem, glm::vec3 curr)
{
  for (size_t i = 0; i < 3; ++i)
  {
    relem.posMin[i] = std::min(relem.posMin[i], static_cast<double>(curr[i]));
    relem.posMax[i] = std::max(relem.posMax[i], static_cast<double>(curr[i]));
  }
}


Baker::ProcessedMeshes Baker::processMeshes(const tinygltf::Model& model) const
{

  ProcessedMeshes result;
  // Pre-allocate enough memory so as not to hit the
  // allocator on the memcpy hotpath
  {
    std::size_t vertexBytes = 0;
    std::size_t indexBytes = 0;
    for (const auto& bufView : model.bufferViews)
    {
      switch (bufView.target)
      {
      case TINYGLTF_TARGET_ARRAY_BUFFER:
        vertexBytes += bufView.byteLength;
        break;
      case TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER:
        indexBytes += bufView.byteLength;
        break;
      default:
        break;
      }
    }
    result.vertices.reserve(vertexBytes / sizeof(Vertex));
    result.indices.reserve(indexBytes / sizeof(std::uint32_t));
  }

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
  }

  result.meshes.reserve(model.meshes.size());


  for (const auto& mesh : model.meshes)
  {
    result.meshes.push_back(Mesh{
      .firstRelem = static_cast<std::uint32_t>(result.relems.size()),
      .relemCount = static_cast<std::uint32_t>(mesh.primitives.size()),
    });

    for (const auto& prim : mesh.primitives)
    {
      if (prim.mode != TINYGLTF_MODE_TRIANGLES)
      {
        spdlog::warn(
          "Encountered a non-triangles primitive, these are not supported for now, skipping it!");
        --result.meshes.back().relemCount;
        continue;
      }

      auto pos_iter = prim.attributes.find("POSITION");

      if (pos_iter == prim.attributes.end())
      {
        spdlog::warn("Encountered a primitive without the position attribute, skipping it!");
        --result.meshes.back().relemCount;
        continue;
      }

      result.relems.push_back(RenderElement{
        .vertexOffset = static_cast<std::uint32_t>(result.vertices.size()),
        .vertexCount = static_cast<std::uint32_t>(model.accessors[pos_iter->second].count),
        .indexOffset = static_cast<std::uint32_t>(result.indices.size()),
        .indexCount = static_cast<std::uint32_t>(model.accessors[prim.indices].count),
        .posMax =
          {std::numeric_limits<double>::min(),
           std::numeric_limits<double>::min(),
           std::numeric_limits<double>::min()},
        .posMin = {
          std::numeric_limits<double>::max(),
          std::numeric_limits<double>::max(),
          std::numeric_limits<double>::max()}});

      const std::size_t vertexCount = model.accessors[pos_iter->second].count;

      result.vertices.resize(result.vertices.size() + vertexCount);
      auto vertexSpan = std::span(result.vertices).last(vertexCount);

      for (const auto& [attr_type, index] : prim.attributes)
      {
        if (attr_type == "NORMAL")
        {
          ProcessAttribute(model, index, vertexSpan, [](Vertex& vtx, const std::byte* ptr) {
            glm::vec3 normal{0};
            std::memcpy(&normal, ptr, sizeof(normal));
            vtx.positionAndNormal.w = std::bit_cast<float>(encode_normal(normal));
          });
        }
        else if (attr_type == "TANGENT")
        {
          ProcessAttribute(model, index, vertexSpan, [](Vertex& vtx, const std::byte* ptr) {
            glm::vec4 tangent{0};
            std::memcpy(&tangent, ptr, sizeof(tangent));
            vtx.texCoordAndTangentAndPadding.z = std::bit_cast<float>(encode_normal(tangent));
          });
        }
        else if (attr_type == "TEXCOORD_0")
        {
          ProcessAttribute(model, index, vertexSpan, [](Vertex& vtx, const std::byte* ptr) {
            glm::vec2 texcoord{0};
            std::memcpy(&texcoord, ptr, sizeof(texcoord));
            vtx.texCoordAndTangentAndPadding.x = texcoord.x; // bit ugly, but it will do
            vtx.texCoordAndTangentAndPadding.y = texcoord.y;
          });
        }
        else if (attr_type == "POSITION")
        {
          ProcessAttribute(model, index, vertexSpan, [&](Vertex& vtx, const std::byte* ptr) {
            glm::vec3 pos{0};
            std::memcpy(&pos, ptr, sizeof(pos));
            vtx.positionAndNormal.x = pos.x; // bit ugly, but it will do
            vtx.positionAndNormal.y = pos.y;
            vtx.positionAndNormal.z = pos.z;
            updateMinMax(result.relems.back(), pos);
          });
        }
        else
        {
          spdlog::warn("Encountered the {} attribute, which is not supported", attr_type);
        }
      }

      auto& index_accessor = model.accessors[prim.indices];

      const std::size_t indexCount = index_accessor.count;
      auto& bufView = model.bufferViews[index_accessor.bufferView];
      auto ptr = reinterpret_cast<const std::byte*>(model.buffers[bufView.buffer].data.data()) +
        bufView.byteOffset + index_accessor.byteOffset;

      if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
      {
        for (std::size_t i = 0; i < indexCount; ++i)
        {
          std::uint16_t index;
          std::memcpy(&index, ptr, sizeof(index));
          result.indices.push_back(index);
          ptr += 2;
        }
      }
      else
      {
        const std::size_t lastTotalIndices = result.indices.size();
        result.indices.resize(lastTotalIndices + indexCount);
        std::memcpy(
          result.indices.data() + lastTotalIndices, ptr, sizeof(result.indices[0]) * indexCount);
      }
    }
  }

  return result;
}

// Currently, the assumptions made about the gtlf file are as follows:
// The file defines only one buffer
// Said buffer is only used for vertex attributes or indices, and not images
// The indices property is defined for all primitives, except the non-triangle or no-position
// ones ( which apparently it doesn't have to be)
// Normals and tangents are defined for all primiteves, except ditto
// No animations or skins are present

void Baker::bakeScene(std::filesystem::path path)
{
  auto maybeModel = loadModel(path);
  if (!maybeModel.has_value())
    return;

  auto model = std::move(*maybeModel);
  auto [verts, inds, relems, meshes] = processMeshes(model);


  // todo check if exists?
  model.extensionsRequired.push_back("KHR_mesh_quantization");
  model.extensionsUsed.push_back("KHR_mesh_quantization");
  if (model.buffers.size() != 1)
  {
    spdlog::warn("The model has more than one buffer, which means something is probably wrong");
  }
  {
    auto& buf = model.buffers[0];
    buf.data.resize(verts.size() * sizeof(Vertex) + inds.size() * sizeof(uint32_t));
    std::memcpy(buf.data.data(), verts.data(), verts.size() * sizeof(Vertex));
    std::memcpy(
      buf.data.data() + verts.size() * sizeof(Vertex), inds.data(), inds.size() * sizeof(uint32_t));
    buf.uri = (path.stem() += "_baked.bin");

    // create file itself?
  }

  model.bufferViews.clear();
  {
    auto& buf = model.bufferViews.emplace_back();
    buf.buffer = 0;
    buf.byteLength = verts.size() * sizeof(Vertex);
    buf.byteOffset = 0;
    buf.byteStride = sizeof(Vertex); // 4
    buf.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  }

  {
    auto& buf = model.bufferViews.emplace_back();
    buf.buffer = 0;
    buf.byteLength = inds.size() * sizeof(uint32_t);
    buf.byteOffset = verts.size() * sizeof(Vertex);
    buf.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  }

  model.accessors.clear();

  {
    auto ind_access = tinygltf::Accessor();
    ind_access.bufferView = 1;
    ind_access.byteOffset = 0; // Base offset
    ind_access.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    ind_access.type = TINYGLTF_TYPE_SCALAR;
    auto pos_access = tinygltf::Accessor();
    pos_access.bufferView = 0;
    pos_access.byteOffset = 0;
    pos_access.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    pos_access.type = TINYGLTF_TYPE_VEC3;
    auto norm_access = tinygltf::Accessor();
    norm_access.bufferView = 0;
    norm_access.byteOffset = 12;
    norm_access.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
    norm_access.normalized = true;
    norm_access.type = TINYGLTF_TYPE_VEC3;
    auto tex_access = tinygltf::Accessor();
    tex_access.bufferView = 0;
    tex_access.byteOffset = 16;
    tex_access.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    tex_access.type = TINYGLTF_TYPE_VEC2;
    auto tan_access = tinygltf::Accessor();
    tan_access.bufferView = 0;
    tan_access.byteOffset = 24;
    tan_access.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
    tan_access.normalized = true;
    tan_access.type = TINYGLTF_TYPE_VEC4;
    std::array vertex_attrs = {pos_access, norm_access, tex_access, tan_access};
    std::array attr_names = {"POSITION", "NORMAL", "TEXCOORD_0", "TANGENT"};

    model.accessors.clear();

    for (size_t i = 0; i < model.meshes.size(); ++i)
    {
      auto& mesh = model.meshes[i];
      std::erase_if(mesh.primitives, [](const auto& prim) {
        return prim.mode != TINYGLTF_MODE_TRIANGLES || !prim.attributes.contains("POSITION");
      });
      for (size_t j = 0; j < mesh.primitives.size(); ++j)
      {
        auto& prim = mesh.primitives[j];
        auto& relem = relems[meshes[i].firstRelem + j];
        vertex_attrs[0].minValues.assign(
          relem.posMin.begin(), relem.posMin.end()); // required, for some reason
        vertex_attrs[0].maxValues.assign(relem.posMax.begin(), relem.posMax.end());

        prim.attributes.clear();
        {
          prim.indices = model.accessors.size();
          auto& curr = model.accessors.emplace_back(ind_access);
          curr.byteOffset += relem.indexOffset * sizeof(uint32_t);
          curr.count = relem.indexCount;
        }
        for (size_t k = 0; k < 4; ++k)
        {
          prim.attributes[attr_names[k]] = model.accessors.size();
          auto& curr = model.accessors.emplace_back(vertex_attrs[k]);
          curr.byteOffset += relem.vertexOffset * sizeof(Vertex);
          curr.count = relem.vertexCount;
        }
      }
    }
  }


  loader.WriteGltfSceneToFile(
    &model, path.parent_path() / path.stem() += "_baked.gltf", false, false, true, false);
}
