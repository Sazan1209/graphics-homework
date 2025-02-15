#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(binding = 1) uniform sampler2D normalMap;

layout(location = 0) in vec2 in_texCoord;

const vec3 grass = vec3(72, 140, 49) / 255.0;
const vec3 dirt = vec3(136, 102, 59) / 255.0;
const float degree = 0.6;

void main()
{
  const vec3 wLightPos = vec3(0, 10, 0);

  const vec3 lightColor = vec3(10.0f, 10.0f, 10.0f);
  const vec3 wNorm = texture(normalMap, in_texCoord).xyz;

  const vec3 surfaceColor = wNorm.y >= degree ? grass : dirt;

  out_color.rgb = surfaceColor;
  out_color.a = 1.0f;
  out_normal.xyz = wNorm.xyz;
}
