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
  CPU_ONLY(float padding[2];)
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
const shader_uvec2 centerGrassCount = shader_uvec2(256, 256);

SHADER_NAMESPACE_END

#endif // TONEMAP_H
