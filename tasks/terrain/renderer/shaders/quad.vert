#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out uint InstanceIndex;

void main() {
  InstanceIndex = gl_InstanceIndex;
}