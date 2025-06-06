#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "static.h"
#include "../utils.glsl"

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_tangent;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in flat int in_comm_pos;

layout(location = 0) out vec4 out_colorMetallic;
layout(location = 1) out vec4 out_normalOcclusion;
layout(location = 2) out vec4 out_emissiveRoughness;

layout(std430, binding = 0) readonly restrict buffer render_buf_0 {
  SingleREIndirectCommand comms[];
};

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main()
{
  MaterialCompat material = comms[in_comm_pos].material;
  {
    vec3 color = material.albedoFactor.xyz;
    if (material.albedoIndex != -1){
      vec4 tmp = texture(textures[material.albedoIndex], in_texcoord);
      color *= toLinear(tmp.xyz);
    }
    out_colorMetallic.xyz = color;
  }
  {
    vec3 normal;
    if (material.normalIndex != -1){
      vec3 tmp = texture(textures[material.normalIndex], in_texcoord).xyz;
      tmp = tmp * 2.0 - 1.0;
      tmp *= material.normalScale;
      tmp = normalize(tmp);

      vec3 bitangent = cross(in_normal, in_tangent);
      mat3 basis = mat3(in_tangent, bitangent, in_normal);
      normal = basis * tmp;
    } else {
      normal = in_normal;
    }
    out_normalOcclusion.xyz = normal;
  }
  {
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if (material.metallicRoughnessIndex != -1){
      vec2 tmp = texture(textures[material.metallicRoughnessIndex], in_texcoord).bg;
      metallic *= tmp.x;
      roughness *= tmp.y;
    }
    out_colorMetallic.w = metallic;
    out_emissiveRoughness.w = roughness;
  }
  {
    vec3 emissive = material.emissiveFactor;
    if (material.emissiveIndex != -1){
      vec3 tmp = texture(textures[material.emissiveIndex], in_texcoord).xyz;
      tmp = toLinear(tmp);
      emissive *= tmp;
    }
    out_emissiveRoughness.xyz = emissive;
  }
  {
    float occlusion = material.occlusionStrength;
    if (material.occlusionIndex != -1){
      occlusion *= texture(textures[material.occlusionIndex], in_texcoord).r;
    }
    out_normalOcclusion.w = occlusion;
  }
}
