#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(binding = 1) uniform sampler2D normalMap;

layout(location = 0) in vec2 inTexCoord;


layout(location = 0) out vec4 out_colorMetallic;
layout(location = 1) out vec4 out_normalOcclusion;
layout(location = 2) out vec4 out_emissiveRoughness;

const vec3 grass = vec3(0, 154, 23) / 255.0;
const vec3 dirt = vec3(58, 44, 31) / 255.0;
const float degree = 0.7;

void main()
{
  vec3 wNorm = texture(normalMap, inTexCoord).xyz;
  // Model to World translation
  wNorm.z = -wNorm.z;

  const vec3 surfaceColor = wNorm.y >= degree ? grass : dirt;

  out_colorMetallic.rgb = surfaceColor;
  out_colorMetallic.a = 0.0f;
  out_normalOcclusion.xyz = wNorm.xyz;
  out_normalOcclusion.w = 1.0f;
  out_emissiveRoughness.xyz = vec3(0);
  out_emissiveRoughness.w = 1.0f;
}
