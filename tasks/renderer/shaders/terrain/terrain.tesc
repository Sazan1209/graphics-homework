#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"
#include "../utils.glsl"

layout(vertices = 2) out;
layout(push_constant) uniform terraintesc_pc
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
  float coef = 1.0 / (1.0 + dist / halfLodDistance);
  return mix(minTess, maxTess, coef);
}

float distToLine(vec2 p, vec2 start, vec2 end)
{
  vec2 dir = end - start;
  float coord = dot(p - start, dir) / dot(dir, dir);
  coord = clamp(coord, 0.0, 1.0);
  return distance(p, start + dir * coord);
}

float distToSquare(vec2 p, vec2 c00, vec2 c10, vec2 c01)
{
  vec2 coord;

  vec2 dir1 = c10 - c00;
  coord.x = dot(p - c00, dir1) / dot(dir1, dir1);

  vec2 dir2 = c01 - c00;
  coord.y = dot(p - c00, dir2) / dot(dir2, dir2);

  coord = clamp(coord, vec2(0.0), vec2(1.0));

  return distance(p, c00 + mat2(dir1, dir2) * coord);
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

  if (Cull(minP, maxP, mProjView))
  {
    for (uint i = 0; i < 4; ++i)
      gl_TessLevelOuter[i] = -1.0;
    gl_TessLevelInner[0] = -1.0;
    gl_TessLevelInner[1] = -1.0;
    return;
  }

  // we only consider the horizontal distance, because the real height of a chunk varies

  const float ol0 = distToLine(eye.xz, corner00.xz, corner01.xz);
  const float ol1 = distToLine(eye.xz, corner00.xz, corner10.xz);
  const float ol2 = distToLine(eye.xz, corner10.xz, corner11.xz);
  const float ol3 = distToLine(eye.xz, corner01.xz, corner11.xz);

  const float centerDist = distToSquare(eye.xz, corner00.xz, corner01.xz, corner10.xz);


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
