#version 450
#extension GL_GOOGLE_include_directive : require

#include "grass.glsl"

layout(push_constant, std430) uniform grassvert_pc
{
  mat4 mDfW;
  //
  vec3 eyePos;
  float bend;
  //
  vec2 windPos;
  float tilt;
  float width;
  //
  float height;
  float midCoef;
  float time;
  float jitterCoef;
  //
  float alignCoef;
  float windChopSize;
  float windTiltStrength;
};

float windStrength(vec3 pos, float time)
{
  return perlin((pos.xz - windPos) / windChopSize) * windTiltStrength;
}

layout(binding = 0, std430) restrict readonly buffer grass_vert_buf0
{
  GrassInstanceData blades[];
};

layout(location = 0) out vec3 out_normal;
layout(location = 1) out float out_t;
layout(location = 2) out float out_x;


vec3 bezierEval(vec3 start, vec3 mid, vec3 end, float t)
{
  float t1 = 1.0 - t;
  float t2 = t;
  vec3 res = t1 * t1 * start;
  res += 2.0 * t1 * t2 * mid;
  res += t2 * t2 * end;
  return res;
}

vec3 bezierGrad(vec3 start, vec3 mid, vec3 end, float t)
{
  float t1 = 1.0 - t;
  float t2 = t;
  vec3 res = t1 * (mid - start);
  res += t2 * (end - mid);
  return res * 2.0;
}

vec2 orth(vec2 v)
{
  v.xy = v.yx;
  v.x = -v.x;
  return v;
}

vec3 align(vec3 val, vec3 dir)
{
  return normalize(val - dot(val, dir) * dir * alignCoef);
}

void main()
{
  GrassInstanceData blade = blades[gl_InstanceIndex];
  float ring = -log2(blade.lod);
  float heightAdj = height * pow(1.2f, ring);
  float widthAdj = width * 1.0 / blade.lod;
  {
    float coef = blade.shouldDissapear ? min(abs(blade.ring + 1.0f - ring), 1.0) : 1.0f;
    heightAdj *= coef;
    widthAdj *= coef;
  }

  uint n = blade.hash;
  vec3 pToEye = normalize(eyePos - blade.pos);
  vec2 facing = vec2(sin(blade.facing), cos(blade.facing));
  float jitter = (sin(time + rand(n) * 3.14 * 2) + 0.8) * jitterCoef;
  float windTilt = windStrength(blade.pos, time);

  // determine start mid end
  vec3 start, mid, end;
  {
    vec2 start2d = vec2(0.0);
    float tiltJitter = (clamp(tilt + jitter + windTilt, -1, 1)) * PI / 2.0;
    float tiltMidCoef = midCoef - jitter / 5.0;
    vec2 end2d = heightAdj * vec2(sin(tiltJitter), cos(tiltJitter));
    vec2 mid2d = mix(start2d, end2d, tiltMidCoef);
    mid2d += bend * orth(end2d - start2d);
    vec2 tmp = facing;
    start = vec3(0);
    mid = vec3(tmp * mid2d.x, mid2d.y).xzy;
    end = vec3(tmp * end2d.x, end2d.y).xzy;
  }

  uint vInd = gl_VertexIndex;
  uint yInd = vInd / 2;
  uint xInd = vInd % 2;
  float vertexNum = blade.ring <= 1.0 ? 7.0 : 3.0;
  float t = (1.0 / vertexNum * float(yInd));
  bool isPoint = yInd == vertexNum;
  float x = isPoint ? 0.0 : (float(xInd) * 2.0 - 1.0);
  float widthLerp = mix(0.0, widthAdj, 1.0 - t * t) * x;
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
