#include "terrain.h"

const vec2 terrainSize = vec2(1024, 1024);
const vec2 squareSize = terrainSize / vec2(gridSize);
const float zScale = 100.0; // The terrain height will range from -200 to 200
const vec3 centerCoordModel = vec3(terrainSize / 2.0, 0).xzy;

const vec3 centerCoordWorld = vec3(0, -5, 0);
// Distance to chunk at which lod will be 1/2
const float halfLodDistance = 48;
const float maxTess = 25.0;
const float minTess = 1.0;
