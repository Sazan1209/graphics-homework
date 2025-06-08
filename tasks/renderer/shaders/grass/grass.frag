#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 in_normal;

layout(location = 0) out vec4 out_colorMetallic;
layout(location = 1) out vec4 out_normalOcclusion;
layout(location = 2) out vec4 out_emissiveRoughness;

const vec3 grass = vec3(0, 154, 23) / 300.0;

void main(){
  out_colorMetallic.xyz = grass;
  out_colorMetallic.w = 0.0;
  out_normalOcclusion.xyz = in_normal;
  out_normalOcclusion.w = 0.0;
  out_emissiveRoughness.xyz = vec3(0);
  out_emissiveRoughness.w = 1.0;
}
