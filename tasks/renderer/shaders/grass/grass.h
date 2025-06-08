#ifndef TONEMAP_H
#define TONEMAP_H

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(grass)

struct GrassInstanceData {
  shader_vec3 pos;
  CPU_ONLY(float padding;)
};

const shader_uvec2 drawnGrassCount = shader_uvec2(128, 128);

SHADER_NAMESPACE_END

#endif // TONEMAP_H
