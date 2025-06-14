#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;

void main()
{
  out_normal = vec4(in_normal, 1.0);
  out_color = vec4(in_color, 1.0);
}
