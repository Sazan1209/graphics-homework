#ifndef STATIC_H
#define STATIC_H

#include "cpp_glsl_compat.h"

// Structure which holds both the indirect rendering command as well as the appropriate rendering
// info
struct Command{
  shader_uint indexCount;
  shader_uint instanceCount;
  shader_uint firstIndex;
  shader_int vertexOffset;
  shader_uint firstInstance;
};

struct MaterialCompat{
  shader_uint albedoIndex;
  shader_uint normalIndex;
  shader_uint emissiveIndex;
  shader_uint metallicRoughnessIndex;
  shader_uint occlusionIndex;
  shader_float normalScale;
  shader_float metallicFactor;
  shader_float roughnessFactor;

  shader_vec4 albedoFactor;
  shader_vec3 emissiveFactor;
  shader_float occlusionStrength;

};

struct SingleREIndirectCommand
{ 
  shader_vec3 min_coord;
  shader_uint matrWfMIndex;
  shader_vec3 max_coord;

  Command command;
  MaterialCompat material;
};

CPU_ONLY(static_assert(sizeof(SingleREIndirectCommand) == 28 * sizeof(float));)

struct GroupREIndirectCommand
{
  shader_vec3 min_coord;
  CPU_ONLY(float padding = -1.0;)
  shader_vec3 max_coord;
  Command command;
  MaterialCompat material;
};
CPU_ONLY(static_assert(sizeof(GroupREIndirectCommand) == 28 * sizeof(float));)

struct REInstanceCullingInfo
{
  shader_uint matrWfMIndex;
  shader_uint commandIndex;
};

struct REInstanceRenderInfo
{
  shader_uint mtwTransformIndex;
};

#endif // STATIC_H
