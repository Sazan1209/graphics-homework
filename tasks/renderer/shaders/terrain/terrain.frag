#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(binding = 1) uniform sampler2D normalMap;

layout(location = 0) in vec2 inTexCoord;

const vec3 grass = vec3(72, 140, 49) / 255.0;
const vec3 dirt = vec3(136, 102, 59) / 255.0;
const float degree = 0.7;

void main()
{
  vec3 wNorm = texture(normalMap, inTexCoord).xyz;
  // Model to World translation
  wNorm.z = -wNorm.z;

  const vec3 surfaceColor = wNorm.y >= degree ? grass : dirt;

  outColor.rgb = surfaceColor;
  outColor.a = 1.0f;
  outNormal.xyz = wNorm.xyz;
}
