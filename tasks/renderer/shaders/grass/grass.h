#ifndef TONEMAP_H
#define TONEMAP_H

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(grass)

struct GrassInstanceData {
  shader_vec3 pos;
  shader_float facing;
  shader_uint hash;
  CPU_ONLY(float padding[3];)
};

const shader_uvec2 drawnGrassCount = shader_uvec2(256, 256);

SHADER_NAMESPACE_END

#endif // TONEMAP_H
