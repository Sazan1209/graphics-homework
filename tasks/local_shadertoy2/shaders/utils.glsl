#ifndef UTILS_GLSL_INCLUDED
#define UTILS_GLSL_INCLUDED


const float pi = 3.1415926535897932384626433;

float vecMin(in vec3 p) { return min(p.x, min(p.y, p.z)); }

float vecMin(in vec2 p) { return min(p.x, p.y); }

mat3 yaw(in float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat3(c, s, 0, -s, c, 0, 0, 0, 1);
}

mat3 pitch(in float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat3(1, 0, 0, 0, c, s, 0, -s, c);
}

mat3 roll(in float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat3(c, 0, s, 0, 1, 0, -s, 0, c);
}


#endif // UTILS_GLSL_INCLUDED
