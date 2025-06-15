#ifndef TONEMAP_H
#define TONEMAP_H

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(grass)

struct GrassInstanceData
{
  shader_vec3 pos;
  shader_float facing;
  shader_uint hash;
  shader_float lod;
  shader_bool shouldDissapear;
  shader_float ring;
};

struct Command
{
  shader_uint vertexCount;
  shader_uint instanceCount;
  shader_uint firstVertex;
  shader_uint firstInstance;
};

struct GrassDrawCallData
{
  Command comm;
};

const shader_uint ringCount = 2;
const shader_uvec2 centerGrassCount = shader_uvec2(512, 512);

SHADER_NAMESPACE_END

#endif // TONEMAP_H
