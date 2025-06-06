#version 450
#extension GL_GOOGLE_include_directive : require

layout(push_constant, std430) uniform grassvert_pc
{
  mat4 mDfW;
  vec3 wPos;
  float bend;
  vec2 facing;
  float tilt;
  float width;
  float height;
  float midCoef;
  float time;
  float jitterCoef;
};

layout(location = 0) out vec3 out_normal;
// layout(location = 1) out float out_t;

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

void main()
{
  uint vInd = gl_VertexIndex;

  // determine start mid end
  vec3 start;
  vec3 mid;
  vec3 end;
  {
    vec2 start2d = vec2(0.0);
    float tiltJitter = tilt + (sin(time) + 1.0) * jitterCoef;
    float tiltMidCoef = midCoef - (sin(time) + 1.0) * jitterCoef / 5.0;
    vec2 end2d = height * vec2(sin(tiltJitter), cos(tiltJitter));
    vec2 mid2d = mix(start2d, end2d, tiltMidCoef);
    {
      vec2 ort = end2d - start2d;
      ort = ort.yx;
      ort.x = -ort.x;
      mid2d += bend * ort;
    }
    vec2 tmp = normalize(facing);
    start = vec3(tmp * start2d.x, start2d.y).xzy;
    mid = vec3(tmp * mid2d.x, mid2d.y).xzy;
    end = vec3(tmp * end2d.x, end2d.y).xzy;
  }
  uint yInd = vInd / 2;
  uint xInd = vInd % 2;
  float t = 1.0 / 7.0 * float(yInd);
  float widthLerp = mix(width, 0.0, t) * (float(xInd) * 2.0 - 1.0);
  vec3 grad = bezierGrad(start, mid, end, t);
  // + or - depending on xInd
  vec3 side;
  {
    vec2 ort = facing.yx;
    ort.x = -ort.x;
    side = vec3(ort, 0).xzy;
  }
  out_normal = normalize(cross(side, grad));
  vec3 point = bezierEval(start, mid, end, t) + side * widthLerp + wPos;
  gl_Position = mDfW * vec4(point, 1.0);
  // out_t = t;
}
