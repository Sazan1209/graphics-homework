#version 460
#extension GL_GOOGLE_include_directive : require
#include "static.h"
#include "unpack_attributes_baked.glsl"


layout(location = 0) in vec4 mPosNorm;
layout(location = 1) in vec4 mTexCoordAndTang;

layout(std430, binding = 0) readonly restrict buffer rendersingle_buf_0 {
  SingleREIndirectCommand comms[];
};

layout(std430, binding = 1) readonly restrict buffer rendersingle_buf_1 {
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

  // vOut.wNorm = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
  // vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
  // vOut.texCoord = vTexCoordAndTang.xy;

  // gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);

  const vec4 mNorm = vec4(decode_normal(floatBitsToUint(mPosNorm.w)), 0.0f);
  const vec4 mTang = vec4(decode_normal(floatBitsToUint(mTexCoordAndTang.z)), 0.0f);
  const mat4 matrWfM = matricesWfM[comms[gl_DrawID].matrWfMIndex];
  out_normal = (matrWfM * mNorm).xyz;
  out_tangent = (matrWfM * mTang).xyz;
  out_texcoord = mTexCoordAndTang.xy;
  out_comm_pos = gl_DrawID;

  gl_Position = matrVfW * matrWfM * vec4(mPosNorm.xyz, 1.0f);
}
