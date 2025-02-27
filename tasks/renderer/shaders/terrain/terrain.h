#ifndef TERRAIN_H
#define TERRAIN_H


// Terrain is generated based on the height map. The corner (0, y, 0) is mapped to (0, 0) on the
// height map, the corner (terrainSize.x, y, -terrainSize.z) is mapped to (heightMap.x, heightMap.y)

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(terrain)

const shader_uvec2 heightMapSize = shader_uvec2(4096, 4096);
const shader_uvec2 gridSize = shader_uvec2(32, 32);

SHADER_NAMESPACE_END

#endif // TERRAIN_H
