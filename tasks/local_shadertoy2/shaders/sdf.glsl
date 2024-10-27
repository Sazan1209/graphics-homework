#ifndef SDF_GLSL_INCLUDED
#define SDF_GLSL_INCLUDED

#extension GL_GOOGLE_include_directive : require

#include "utils.glsl"

float sdCube(in vec3 p, in vec3 param) {
  vec3 dim = param / 2.0;
  p = abs(p);
  vec3 d = p - dim;
  d = max(d, vec3(0.0));
  float l = length(d);
  if (abs(l) <= 0.0001) {
    return -vecMin(dim - p);
  }
  return l;
}

////////////////////////////////////////////////////////

float sdRectangle(in vec2 p, in vec2 param) {
  vec2 dim = param / 2.0;
  p = abs(p);
  vec2 d = p - dim;
  d = max(d, vec2(0.0, 0.0));
  float l = length(d);
  if (abs(l) <= 0.0001) {
    return -vecMin(dim - p);
  }
  return l;
}

float sdCyllinder(in vec3 p, in vec2 param) {
  return sdRectangle(vec2(length(p.xy), p.z), param);
}

////////////////////////////////////////////////////////

float sdCircle(in vec2 p, in float param) { return length(p) - param; }

float sum(in vec3 p) { return dot(p, vec3(1, 1, 1)); }

float sum(in vec2 p) { return dot(p, vec2(1, 1)); }

float sdSphere(in vec3 p, in float r) { return length(p) - r; }


#endif // SDF_GLSL_INCLUDED
