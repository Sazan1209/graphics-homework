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

struct SingleREIndirectCommand
{ 
  shader_vec3 min_coord;
  shader_uint matrWfMIndex;
  shader_vec3 max_coord;

  Command command;
};
CPU_ONLY(static_assert(sizeof(SingleREIndirectCommand) == 12 * sizeof(float));)

struct GroupREIndirectCommand
{
  shader_vec3 min_coord;
  CPU_ONLY(float padding;)
  shader_vec3 max_coord;
  Command command;
};
CPU_ONLY(static_assert(sizeof(GroupREIndirectCommand) == 12 * sizeof(float));)

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
