#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec4 out_colorMetallic;
layout(location = 1) out vec4 out_normalOcclusion;
layout(location = 2) out vec4 out_emissiveRoughness;

void main()
{
  out_normalOcclusion = vec4(in_normal, 0.0);
  out_colorMetallic = vec4(1.0, 1.0, 1.0, 1.0);
  out_emissiveRoughness = vec4(in_color, 1.0);
}
