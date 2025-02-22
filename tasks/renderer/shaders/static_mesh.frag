#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;

layout(location = 0) in in_vs_out
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
};

void main()
{
  const vec3 surfaceColor = vec3(1.0f, 1.0f, 1.0f);

  out_color.rgb = surfaceColor;
  out_color.a = 1.0f;
  out_normal.xyz = wNorm;
}
