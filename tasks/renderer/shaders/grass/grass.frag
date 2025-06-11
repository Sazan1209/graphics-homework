#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 in_normal;
layout(location = 1) in float in_t;
layout(location = 2) in float in_x;

layout(location = 0) out vec4 out_colorMetallic;
layout(location = 1) out vec4 out_normalOcclusion;
layout(location = 2) out vec4 out_emissiveRoughness;

const vec3 grass = vec3(0, 154, 23) / 255.0;

void main(){
  out_colorMetallic.xyz = grass;
  out_colorMetallic.w = 0.0;
  out_normalOcclusion.xyz = gl_FrontFacing ? in_normal : -in_normal;
  out_normalOcclusion.w = mix(0.3, 1.0, in_t);
  out_emissiveRoughness.xyz = vec3(0);
  float coef = clamp((abs(in_x) - 0.3) * 3, 0.0, 1.0);
  out_emissiveRoughness.w = mix(0.8, 0.6, coef);
}
