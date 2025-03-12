#version 450
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes_baked.glsl"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform staticvert_pc
{
  mat4 mProjView;
}
params;

layout(std430, binding = 0) restrict readonly buffer matr
{
  mat4 mModels[];
};

layout(location = 0) out in_vs_out
{
  vec3 wNorm;
  vec2 texCoord;
}
vOut;

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  const vec3 wNorm = decode_normal(floatBitsToUint(vPosNorm.w));
  const mat4 mModel = mModels[gl_InstanceIndex];

  vOut.wNorm = normalize(mat3(mModel) * wNorm.xyz);
  vOut.texCoord = vTexCoordAndTang.xy;

  vec3 wPos = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
  gl_Position = params.mProjView * vec4(wPos, 1.0);
}
