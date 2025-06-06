#version 460
#extension GL_GOOGLE_include_directive : require
#include "static.h"
#include "unpack_attributes_baked.glsl"

layout(location = 0) in vec4 mPosNorm;
layout(location = 1) in vec4 mTexCoordAndTang;

layout(std430, binding = 1) readonly restrict buffer rendergroup_buf_1 {
  uint matrWfMIndices[];
};

layout(std430, binding = 2) readonly restrict buffer rendergroup_buf_2 {
  mat4 matricesWfM[];
};

layout(push_constant) uniform rendersingle_pc{
  mat4 matrVfW;
};

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_tangent;
layout(location = 2) out vec2 out_texcoord;
layout(location = 3) out int out_comm_pos;

out gl_PerVertex
{
  vec4 gl_Position;
};

void main(){
  const vec4 mNorm = vec4(decode_normal(floatBitsToUint(mPosNorm.w)), 0.0f);
  const vec4 mTang = vec4(decode_normal(floatBitsToUint(mTexCoordAndTang.z)), 0.0f);
  const mat4 matrWfM = matricesWfM[matrWfMIndices[gl_InstanceIndex]];
  out_normal = (matrWfM * mNorm).xyz;
  out_tangent = (matrWfM * mTang).xyz;
  out_texcoord = mTexCoordAndTang.xy;
  out_comm_pos = gl_DrawID;
  gl_Position = matrVfW * matrWfM * vec4(mPosNorm.xyz, 1.0f);
}
