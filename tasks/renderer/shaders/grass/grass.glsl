#include "grass.h"

const vec2 terrainSize = vec2(1024, 1024);
const vec3 centerCoordModel = vec3(terrainSize / 2.0, 0).xzy;
const vec3 centerCoordWorld = vec3(0, -5.0, 0);

// zInv(eyeW - centerW) + centerM = eyeM
#define PI 3.1415926538

// the size of the square around the player in which grass is rendered;
const vec2 centerSize = vec2(64, 64);
const vec2 grassPerMeter = vec2(centerGrassCount) / centerSize;

const vec2 totalGrassCount = terrainSize * grassPerMeter;

const vec2 windDir = vec2(0.0, 1.0);

float rand(inout uint n)
{
  n *= 43758;
  return fract(sin(n % 4096));
}

vec2 randomGradient(in ivec2 coord)
{
  const uint w = 32u;
  const uint s = w / 2u; // rotation width
  uint a = uint(coord.x);
  uint b = uint(coord.y);
  a *= 3284157443u;
  b ^= a << s | a >> w - s;
  b *= 1911520717u;
  a ^= b << s | b >> w - s;
  a *= 2048419325u;
  float random = float(a) * (3.14159265 / float(~(~0u >> 1u))); // in [0, 2*Pi]
  vec2 v;
  v.x = cos(random);
  v.y = sin(random);
  return v;
}

float dotGridGradient(in vec2 pixCoord, in ivec2 gridCoord)
{
  vec2 gradient = randomGradient(gridCoord);
  vec2 offset = pixCoord - vec2(gridCoord);

  return dot(offset, gradient);
}

float perlin(in vec2 pixCoord)
{

  int x0 = int(floor(pixCoord.x));
  int x1 = x0 + 1;
  int y0 = int(floor(pixCoord.y));
  int y1 = y0 + 1;

  vec2 s = pixCoord - vec2(x0, y0);
  s = (3.0 - s * 2.0) * s * s;

  float n0 = dotGridGradient(pixCoord, ivec2(x0, y0));
  float n1 = dotGridGradient(pixCoord, ivec2(x1, y0));
  float ix0 = mix(n0, n1, s.x);

  n0 = dotGridGradient(pixCoord, ivec2(x0, y1));
  n1 = dotGridGradient(pixCoord, ivec2(x1, y1));
  float ix1 = mix(n0, n1, s.x);

  return mix(ix0, ix1, s.y);
}
