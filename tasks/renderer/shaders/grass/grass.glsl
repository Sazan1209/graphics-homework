#include "grass.h"

const vec2 terrainSize = vec2(1024, 1024);
const vec3 centerCoordModel = vec3(terrainSize / 2.0, 0).xzy;
const vec3 centerCoordWorld = vec3(0, -100, 0);

// zInv(eyeW - centerW) + centerM = eyeM


// the size of the square around the player in which grass is rendered;
const vec2 grassDrawSquare = vec2(32, 32);
const vec2 grassPerMeter = vec2(drawnGrassCount) / grassDrawSquare;

const vec2 totalGrassCount = terrainSize * grassPerMeter;
