#version 450
#extension GL_GOOGLE_include_directive : require

#include "grass.glsl"

layout(push_constant, std430) uniform grassvert_pc
{
  mat4 mDfW;
  vec3 eyePos;
  float bend;
  float tilt;
  float width;
  float height;
  float midCoef;
  float time;
  float jitterCoef;
  float alignCoef;
};

layout(binding = 0, std430) restrict readonly buffer grass_vert_buf0
{
  GrassInstanceData blades[];
};

layout(location = 0) out vec3 out_normal;
layout(location = 1) out float out_t;
layout(location = 2) out float out_x;


vec3 bezierEval(vec3 start, vec3 mid, vec3 end, float t){
  float t1 = 1.0 - t;
  float t2 = t;
  vec3 res = t1 * t1 * start;
  res += 2.0 * t1 * t2 * mid;
  res += t2 * t2 * end;
  return res;
}

vec3 bezierGrad(vec3 start, vec3 mid, vec3 end, float t){
  float t1 = 1.0 - t;
  float t2 = t;
  vec3 res = t1 * (mid - start);
  res += t2 * (end - mid);
  return res * 2.0;
}

vec2 orth(vec2 v){
  v.xy = v.yx;
  v.x = -v.x;
  return v;
}

vec3 align(vec3 val, vec3 dir){
  return val - dot(val, dir) * dir * alignCoef;
}

void main()
{
  uint vInd = gl_VertexIndex;
  GrassInstanceData blade = blades[gl_InstanceIndex];
  uint n = blade.hash;
  vec3 pToEye = normalize(eyePos - blade.pos);

  vec2 facing = vec2(sin(blade.facing), cos(blade.facing));
  float jitter = (sin(time + rand(n) * 3.14 * 2) + 1.0) * jitterCoef;

  // determine start mid end
  vec3 start;
  vec3 mid;
  vec3 end;
  {
    vec2 start2d = vec2(0.0);
    float tiltJitter = (tilt + jitter) * PI / 2.0;
    float tiltMidCoef = midCoef - jitter / 5.0;
    vec2 end2d = height * vec2(sin(tiltJitter), cos(tiltJitter));
    vec2 mid2d = mix(start2d, end2d, tiltMidCoef);
    mid2d += bend * orth(end2d - start2d);
    vec2 tmp = facing;
    start = vec3(tmp * start2d.x, start2d.y).xzy;
    mid = vec3(tmp * mid2d.x, mid2d.y).xzy;
    end = vec3(tmp * end2d.x, end2d.y).xzy;
  }
  uint yInd = vInd / 2;
  uint xInd = vInd % 2;
  float t = (1.0 / 7.0 * float(yInd));
  float x = yInd == 7 ? 0.0 : (float(xInd) * 2.0 - 1.0);
  float widthLerp = mix(0.0, width,  1.0 - t * t) * x;
  vec3 grad = bezierGrad(start, mid, end, t);
  vec3 side = vec3(orth(facing), 0).xzy;
  side = align(side, pToEye);
  vec3 normal = normalize(cross(side, grad));

  vec3 point = bezierEval(start, mid, end, t) + side * widthLerp + blade.pos;
  normal = mix(normal, side * x, 0.2);
  out_normal = normalize(normal);
  gl_Position = mDfW * vec4(point, 1.0);
  out_t = t;
  out_x = x;
}
