#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, fractional_even_spacing, cw) in;

layout(binding = 0) uniform sampler2D perlinNoise;
layout(push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 eye;
};

layout(location = 0) in vec3 inWorldPos[];
layout(location = 1) in vec2 inTexCoord[];

layout(location = 0) out vec2 outTexCoord;

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  vec3 wPos;
  wPos.xz = mix(inWorldPos[0].xz, inWorldPos[1].xz, gl_TessCoord.xy);
  outTexCoord = mix(inTexCoord[0], inTexCoord[1], gl_TessCoord.xy);
  wPos.y = inWorldPos[0].y + texture(perlinNoise, outTexCoord).x;
  gl_Position = mProjView * vec4(wPos, 1.0);
}
