#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"
#include "markdown.glsl"
#include "sdf.glsl"

layout(push_constant, std430) uniform params
{
  UniformParams uni_params;
};


// enum materials
#define NONE 0
#define BRASS 1
#define SUN 2
#define PLANET1 3
#define PLANET2 4
#define PLANET3 5
#define PLANET4 6

float sdE(in vec3 p, out int mat_num, out mat4 tf){
    float l = 10000.0;
    mat_num = NONE;
    BEGIN_GROUP()

        //Floor
        BEGIN_FIG()
        F_SHIFT(0, 0, -5)
        END_FIG(sdCube, vec3(15, 15, 1))

        // Podium + main pole

        BEGIN_FIG()
        F_SHIFT(0, 0, -4.5)
        END_FIG_MAT(sdCyllinder, vec2(3.5, 1), BRASS)

        BEGIN_FIG()
        F_SHIFT(0, 0, -3.)
        END_FIG_MAT(sdCyllinder, vec2(.5, 2), BRASS)

        BEGIN_FIG()
        F_SHIFT(0, 0, -1.0)
        END_FIG_MAT(sdSphere, 1.0, SUN)

    END_GROUP()

    // Planet 1
    BEGIN_GROUP()
        F_YAW(uni_params.time)
        BEGIN_FIG()
        F_SHIFT(3.25, 0, -3.75)
        F_ROLL(pi / 2.0)
        END_FIG_MAT(sdCyllinder, vec2(.25, 6), BRASS)

        BEGIN_FIG()
        F_SHIFT(2.25 + 4.0, 0.0, -2.25 + 0.375 - 1.5)
        END_FIG_MAT(sdCyllinder, vec2(.25, 0.75), BRASS)

        BEGIN_FIG()
        F_SHIFT(2.25 + 4.0, 0.0, -1.0 - 1.5)
        END_FIG_MAT(sdSphere, .4, PLANET1)

    END_GROUP()

    // Planet 2
    BEGIN_GROUP()
        F_YAW(-uni_params.time / 2.0)
        BEGIN_FIG()
        F_SHIFT(2.75 - .3333 / 2.0, 0, -3.25)
        F_ROLL(pi / 2.0)
        END_FIG_MAT(sdCyllinder, vec2(.25, 4.6666), BRASS)

        BEGIN_FIG()
        F_SHIFT(2.25 + 2.6666, 0.0, -2.25 + 0.375 - 1.0)
        END_FIG_MAT(sdCyllinder, vec2(.25, 0.75), BRASS)

        BEGIN_FIG()
        F_SHIFT(2.25 + 2.6666, 0.0, -1.0 - 1.0)
        END_FIG_MAT(sdSphere, .7, PLANET2)
    END_GROUP()

    // Planet 3
    BEGIN_GROUP()

        F_YAW(-uni_params.time)
        BEGIN_FIG()
        F_SHIFT(2.25 - .33333, 0, -2.75)
        F_ROLL(pi / 2.0)
        END_FIG_MAT(sdCyllinder, vec2(.25, 3.3333), 1)

        BEGIN_FIG()
        F_SHIFT(2.25 + 1.3333, 0.0, -2.25 + 0.375 - .5)
        END_FIG_MAT(sdCyllinder, vec2(.25, 0.75), 1)

        BEGIN_FIG()
        F_SHIFT(2.25 + 1.3333, 0.0, -1.0 - .5)
        END_FIG_MAT(sdSphere, .5, PLANET3)

    END_GROUP()


    // Planet 4
    BEGIN_GROUP()

        F_YAW(uni_params.time*1.5)
        BEGIN_FIG()
        F_SHIFT(1.25, 0, -2.25)
        F_ROLL(pi / 2.0)
        END_FIG_MAT(sdCyllinder, vec2(.25, 2), 1)

        BEGIN_FIG()
        F_SHIFT(2.25, 0.0, -2.25 + 0.375)
        END_FIG_MAT(sdCyllinder, vec2(.25, 0.75), 1)

        BEGIN_FIG()
        F_SHIFT(2.25, 0.0, -1.0)
        END_FIG_MAT(sdSphere, .5, PLANET4)
    END_GROUP()

    return l;
}

float sdE(vec3 p)
{
  int mat;
  mat4 tf;
  return sdE(p, mat, tf);
}

#define MAX_ITERS 90
#define MAX_DIST 100.0
vec3 trace(vec3 from, vec3 dir, out bool hit, out int mat_num, out mat4 tf)
{
  vec3 p = from;
  float totalDist = 0.0;

  hit = false;

  for (int steps = 0; steps < MAX_ITERS; steps++)
  {
    float dist = sdE(p, mat_num, tf);

    if (dist < 0.01)
    {
      hit = true;
      break;
    }

    totalDist += dist;

    if (totalDist > MAX_DIST)
      break;

    p += dist * dir;
  }

  return p;
}

vec3 trace(vec3 from, vec3 dir, out bool hit){
    int mat_num;
    mat4 tf;
    return trace(from, dir, hit, mat_num, tf);
}

vec3 generateNormal(vec3 z, float d)
{
  float e = max(d * 0.5, 0.0001);
  float dx1 = sdE(z + vec3(e, 0, 0));
  float dx2 = sdE(z - vec3(e, 0, 0));
  float dy1 = sdE(z + vec3(0, e, 0));
  float dy2 = sdE(z - vec3(0, e, 0));
  float dz1 = sdE(z + vec3(0, 0, e));
  float dz2 = sdE(z - vec3(0, 0, e));

  return normalize(vec3(dx1 - dx2, dy1 - dy2, dz1 - dz2));
}

float calculate_light(in vec3 p, in float shine, in vec3 normal)
{
  vec3 light_v = normalize(uni_params.lightPos - p);
  bool hit;
  float ambient = 0.2;
  trace(p + normal * 0.1, light_v, hit);
  if (hit)
  {
    return ambient;
  }
  vec3 eye_v = normalize(uni_params.cam_pos - p);
  float diffuse = max(0.0, dot(normal, light_v));
  vec3 refract_v = normalize(eye_v + light_v);
  float highlight = max(0.0, dot(refract_v, normal));
  return diffuse + pow(highlight, shine) + ambient;
}

layout (binding = 0) uniform sampler2D brassSampler;

vec4 brass_texture(in vec3 p, in vec3 n){
    n = abs(n);
    p /= 20.0;
    mat3x4 t = mat3x4(texture(brassSampler, p.yz), texture(brassSampler, p.zx), texture(brassSampler, p.xy));
    return t * n;
}

layout (binding = 1) uniform sampler2D procSampler;

vec4 proc_texture(in vec3 p, in vec3 n){
    n = abs(n);
    p /= 10.0;
    mat3x4 t = mat3x4(texture(procSampler, p.yz), texture(procSampler, p.zx), texture(procSampler, p.xy));
    return t * n;
}

vec4 calc_color(in vec3 p, in int mat_num, in mat4 tf){
    vec3 color = vec3(1, 1, 1);
    vec3 normal = generateNormal(p, 0.001);
    vec3 normal_rel = mat3(tf) * normal;
    vec3 p_rel = vec3(tf * vec4(p, 1));
    float shine = 5;

    BEGIN_MAT(NONE)
    END_MAT()

    BEGIN_MAT(BRASS)
    COLOR(81, 64, 26)
    TEXTURE(brass_texture)
    SHINE(50)
    END_MAT()

    BEGIN_MAT(SUN)
    COLOR(100, 32, 0)
    SHINE(5)
    TEXTURE(proc_texture)
    END_MAT()

    BEGIN_MAT(PLANET1)
    COLOR(0, 8, 100)
    //TEXTURE(0)
    SHINE(5)
    END_MAT()

    BEGIN_MAT(PLANET2)
    COLOR(100, 0, 0)
    TEXTURE(brass_texture)
    SHINE(5)
    END_MAT()

    BEGIN_MAT(PLANET3)
    COLOR(0, 100, 92)
    SHINE(5)
    //TEXTURE(0)
    END_MAT()

    BEGIN_MAT(PLANET4)
    COLOR(8, 100, 0)
    //TEXTURE(0)
    SHINE(5)
    END_MAT()
}

vec3 screen_to_world(in vec2 sc)
{
  return mat3(uni_params.camera) * vec3(sc, 1);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
  vec3 p = screen_to_world(fragCoord);
  mat4 tf;
  int mat_num;
  bool hit;

  p = trace(p, normalize(p - uni_params.cam_pos), hit, mat_num, tf);
  if (hit)
  {
    fragColor = calc_color(p, mat_num, tf);
  }
  else
  {
    fragColor = vec4(0.1, 0.1, 1.0, 1.0);
  }
}

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout (location = 0) out vec4 color;

void main()
{
  mainImage(color, surf.texCoord);
}
