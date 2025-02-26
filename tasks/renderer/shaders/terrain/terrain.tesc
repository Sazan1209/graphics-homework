#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"

layout(vertices = 2) out;
layout(push_constant) uniform pc
{
  mat4 mProjView;
  vec3 eye;
};

layout(location = 0) in uint inInstanceIndex[];

layout(location = 0) out vec3 outWorldPos[];
layout(location = 1) out vec2 outTexCoord[];

vec3 calcPosWorld(in uint cornerNum)
{
  const uint ind = inInstanceIndex[0];
  vec2 sqrCoord = vec2(ind % gridSize.x, ind / gridSize.x);
  vec2 cornerOffset = vec2(cornerNum / 2u, cornerNum % 2);
  vec3 cornerPosModel = vec3((sqrCoord + cornerOffset) * squareSize, 0).xzy;

  // The transformation is done such that the center is at centerPosWorld afterwards
  vec3 cornerPosWorld = cornerPosModel - centerCoordModel;
  cornerPosWorld.z = -cornerPosWorld.z;
  cornerPosWorld += centerCoordWorld;

  return cornerPosWorld;
}

vec2 calcTexcoord(in uint cornerNum)
{
  const uint ind = inInstanceIndex[0];
  vec2 sqrCoord = vec2(ind % gridSize.x, ind / gridSize.x);
  vec2 cornerOffset = vec2(cornerNum / 2u, cornerNum % 2u);
  return (sqrCoord + cornerOffset) / vec2(gridSize);
}

// Tesselation level is inversely proportional to distance, so that an object twice as small on the
// screen has a tess level that's twice as small

float GetTessLevel(in float dist)
{
  float coef = 1.0 / (1.0 + dist * tessCoefficient);
  return mix(maxTess, minTess, coef);
}

bool Cull(vec3 minP, vec3 maxP)
{

  vec3 minV;
  vec3 maxV;
  {
    vec4 proj = mProjView * vec4(minP, 1);
    proj /= abs(proj.w);
    minV = proj.xyz;
    maxV = proj.xyz;
  }
  for (uint mask = 1; mask < 8u; ++mask)
  {
    vec3 point;
    for (uint i = 0; i < 3; ++i)
    {
      point[i] = ((mask & (1u << i)) > 0) ? maxP[i] : minP[i];
    }
    vec4 corner = mProjView * vec4(point, 1);
    corner /= abs(corner.w);
    minV = min(minV, corner.xyz);
    maxV = max(maxV, corner.xyz);
  }
  return any(lessThan(maxV, vec3(-1, -1, 0))) || any(greaterThan(minV, vec3(1, 1, 1)));
}

void main()
{
  const vec3 corner00 = calcPosWorld(0);
  const vec3 corner01 = calcPosWorld(1);
  const vec3 corner10 = calcPosWorld(2);
  const vec3 corner11 = calcPosWorld(3);

  vec3 minP = corner01;
  minP.y += -zScale;
  vec3 maxP = corner10;
  maxP.y += zScale;

  if (Cull(minP, maxP))
  {
    for (uint i = 0; i < 4; ++i)
      gl_TessLevelOuter[i] = -1.0;
    gl_TessLevelInner[0] = -1.0;
    gl_TessLevelInner[1] = -1.0;
    return;
  }

  // we only consider the horizontal distance, because the real height of a chunk varies
  const float dist00 = distance(corner00.xz, eye.xz);
  const float dist01 = distance(corner01.xz, eye.xz);
  const float dist10 = distance(corner10.xz, eye.xz);
  const float dist11 = distance(corner11.xz, eye.xz);

  const float ol0 = min(dist00, dist01);
  const float ol1 = min(dist00, dist10);
  const float ol2 = min(dist10, dist11);
  const float ol3 = min(dist01, dist11);

  const float centerDist = min(ol0, ol3);

  gl_TessLevelOuter[0] = GetTessLevel(ol0);
  gl_TessLevelOuter[1] = GetTessLevel(ol1);
  gl_TessLevelOuter[2] = GetTessLevel(ol2);
  gl_TessLevelOuter[3] = GetTessLevel(ol3);
  float centerTess = GetTessLevel(centerDist);
  gl_TessLevelInner[0] = centerTess;
  gl_TessLevelInner[1] = centerTess;

  outWorldPos[gl_InvocationID] = gl_InvocationID == 0 ? corner00 : corner11;
  outTexCoord[gl_InvocationID] = gl_InvocationID == 0 ? calcTexcoord(0) : calcTexcoord(3);
}
