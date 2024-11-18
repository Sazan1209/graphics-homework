#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, ccw) in;

layout(binding = 0) uniform sampler2D perlinNoise;
layout(push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 eye;
};

const float zScale = 200.0;

layout(location = 0) in vec3 WorldPos_ES_in[];
layout(location = 1) in vec2 TexCoord_ES_in[];

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
};

out gl_PerVertex
{
  vec4 gl_Position;
};

vec3 calcNorm(vec2 texCoord){
  float left = textureOffset(perlinNoise, texCoord, ivec2(-1, 0)).x;
  float right = textureOffset(perlinNoise, texCoord, ivec2(1, 0)).x;
  float down = textureOffset(perlinNoise, texCoord, ivec2(0, -1)).x;
  float up = textureOffset(perlinNoise, texCoord, ivec2(0, 1)).x;
  return normalize(vec3(left - right, down - up, 2.0));
}

void main()
{
  wPos = mix(WorldPos_ES_in[0], WorldPos_ES_in[1], vec3(gl_TessCoord.xy, 0).xzy);
  texCoord = mix(TexCoord_ES_in[0], TexCoord_ES_in[1], gl_TessCoord.xy);
  wPos.y += texture(perlinNoise, texCoord).x * zScale;
  wTangent = vec3(0, 0, 1);
  wNorm = calcNorm(texCoord);
  gl_Position = mProjView * vec4(wPos, 1.0);
}
