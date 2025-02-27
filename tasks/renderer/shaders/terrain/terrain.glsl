#include "terrain.h"

const vec2 terrainSize = vec2(1024, 1024);
const vec2 squareSize = terrainSize / vec2(gridSize);
const float zScale = 100.0; // The terrain height will range from -200 to 200

const vec3 centerCoordWorld = vec3(0, -200, 0);
const vec3 centerCoordModel = vec3(terrainSize / 2.0, 0).xzy;

const float tessCoefficient = 1.0 / 50.0;

const float maxTess = 20.0;
const float minTess = 1.0;
