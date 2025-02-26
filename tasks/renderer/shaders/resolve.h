#ifndef RESOLVE_H
#define RESOLVE_H

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(resolve)

struct PointLight
{
  shader_vec3 color;
  shader_float strength;
  shader_vec3 pos;
  shader_float padding;
};

struct Sunlight
{
  shader_vec3 dir;
  shader_float strength;
  shader_vec3 color;
  shader_float ambient;
};

SHADER_NAMESPACE_END

#endif // RESOLVE_H
