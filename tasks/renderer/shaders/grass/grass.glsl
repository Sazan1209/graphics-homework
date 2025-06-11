#include "grass.h"

const vec2 terrainSize = vec2(1024, 1024);
const vec3 centerCoordModel = vec3(terrainSize / 2.0, 0).xzy;
const vec3 centerCoordWorld = vec3(0, -5.0, 0);

// zInv(eyeW - centerW) + centerM = eyeM
#define PI 3.1415926538

// the size of the square around the player in which grass is rendered;
const vec2 centerSize = vec2(32, 32);
const vec2 grassPerMeter = vec2(centerGrassCount) / centerSize;

const vec2 totalGrassCount = terrainSize * grassPerMeter;



float rand(inout uint n)
{
  n *= 43758;
  return fract(sin(n % 4096));
}
