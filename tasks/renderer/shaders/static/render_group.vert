#version 450
#extension GL_GOOGLE_include_directive : require
#include "static.h"
#include "unpack_attributes_baked.glsl"

layout(location = 0) in vec4 mPosNorm;
layout(location = 1) in vec4 mTexCoordAndTang;

layout(std430, binding = 0) readonly restrict buffer rendersingle_buf_0 {
  uint matrWfMIndices[];
};

layout(std430, binding = 1) readonly restrict buffer rendersingle_buf_1 {
  mat4 matricesWfM[];
};

layout(push_constant) uniform rendersingle_pc{
  mat4 matrVfW;
};

layout(location = 0) out vs_out
{
  vec3 wNorm;
};

out gl_PerVertex
{
  vec4 gl_Position;
};

void main(){
  const vec4 mNorm = vec4(decode_normal(floatBitsToUint(mPosNorm.w)), 0.0f);
  const mat4 matrWfM = matricesWfM[matrWfMIndices[gl_InstanceIndex]];
  wNorm = (matrWfM * mNorm).xyz;
  gl_Position = matrVfW * matrWfM * vec4(mPosNorm.xyz, 1.0f);
}
