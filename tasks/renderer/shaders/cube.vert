#version 450
#extension GL_GOOGLE_include_directive : require

#include "resolve.h"

layout(binding = 0, std430) readonly restrict buffer cubevert_buf
{
  PointLight lights[];
};

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_color;

layout(push_constant) uniform cubevert_pc
{
  mat4 mProjView;
};

vec3 cubeVertices[] = {
  vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, -1.0f, 1.0f),  vec3(-1.0f, 1.0f, 1.0f), // left
  vec3(1.0f, 1.0f, -1.0f),   vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 1.0f, -1.0f), // back
  vec3(1.0f, -1.0f, 1.0f),   vec3(-1.0f, -1.0f, -1.0f), vec3(1.0f, -1.0f, -1.0f), // bottom
  vec3(1.0f, 1.0f, -1.0f),   vec3(1.0f, -1.0f, -1.0f),  vec3(-1.0f, -1.0f, -1.0f), // back
  vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 1.0f, 1.0f),   vec3(-1.0f, 1.0f, -1.0f), // left
  vec3(1.0f, -1.0f, 1.0f),   vec3(-1.0f, -1.0f, 1.0f),  vec3(-1.0f, -1.0f, -1.0f), // bottom
  vec3(-1.0f, 1.0f, 1.0f),   vec3(-1.0f, -1.0f, 1.0f),  vec3(1.0f, -1.0f, 1.0f), // front
  vec3(1.0f, 1.0f, 1.0f),    vec3(1.0f, -1.0f, -1.0f),  vec3(1.0f, 1.0f, -1.0f), // right
  vec3(1.0f, -1.0f, -1.0f),  vec3(1.0f, 1.0f, 1.0f),    vec3(1.0f, -1.0f, 1.0f), // right
  vec3(1.0f, 1.0f, 1.0f),    vec3(1.0f, 1.0f, -1.0f),   vec3(-1.0f, 1.0f, -1.0f), // top
  vec3(1.0f, 1.0f, 1.0f),    vec3(-1.0f, 1.0f, -1.0f),  vec3(-1.0f, 1.0f, 1.0f), // top
  vec3(1.0f, 1.0f, 1.0f),    vec3(-1.0f, 1.0f, 1.0f),   vec3(1.0f, -1.0f, 1.0f)}; // front


vec3 normals[] = {
  vec3(1, 0, 0),
  vec3(0, 0, 1),
  vec3(0, -1, 0),
  vec3(0, 0, 1),
  vec3(1, 0, 0),
  vec3(0, -1, 0),
  vec3(0, 0, -1),
  vec3(-1, 0, 0),
  vec3(-1, 0, 0),
  vec3(0, 1, 0),
  vec3(0, 1, 0),
  vec3(0, 0, -1),
};

void main()
{
  uint vInd = gl_VertexIndex;
  uint iInd = gl_InstanceIndex;

  out_normal = normals[vInd / 3];
  out_color = lights[iInd].color;
  gl_Position = mProjView * vec4(cubeVertices[vInd] + lights[iInd].pos, 1.0);
}
