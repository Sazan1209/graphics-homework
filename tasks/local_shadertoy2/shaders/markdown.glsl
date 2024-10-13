#ifndef MARKDOWN_GLSL_INCLUDED
#define MARKDOWN_GLSL_INCLUDED

#extension GL_ARB_separate_shader_objects : enable

struct material {
  vec3 color;
  float roughness;
};

// clang-format off

#define BEGIN_GROUP()       \
{                           \
  float l0 = l;             \
  vec3 p0 = p;              \
  float total_scale0 = 1.0; \
  int material0 = 0;

#define BEGIN_FIG()           \
  {                           \
    vec3 p0 = p0;             \
    float total_scale0 = 1.0; \
    float smooth0 = 0.0;

#define F_YAW(angle) p0 = transpose(yaw(angle)) * p0;

#define F_PITCH(angle) p0 = transpose(pitch(angle)) * p0;

#define F_ROLL(angle) p0 = transpose(roll(angle)) * p0;

#define F_SCALE(scale) \
  p0 /= scale;         \
  total_scale0 *= scale;

#define F_SMOOTH(smooth) smooth0 = smooth;

#define F_SHIFT(x, y, z) p0 -= vec3(x, y, z);

#define END_FIG(sdF, param)                              \
    float tmp = sdF(p0, param) * total_scale0 - smooth0; \
    if (tmp < l0) {                                      \
      l0 = tmp;                                          \
      material0 = 0;                                     \
    }                                                    \
  }

#define END_FIG_MAT(sdF, param, num)                   \
    float tmp = sdF(p0, param) * total_scale0 - smooth0; \
    if (tmp < l0) {                                      \
      l0 = tmp;                                          \
      material0 = num;                                   \
    }                                                    \
  }

#define END_GROUP()      \
  l0 *= total_scale0;    \
  if (l0 < l) {          \
    l = l0;              \
    mat_num = material0; \
  }                      \
}

#define BEGIN_MAT(num) if (mat_num == num) {
#define COLOR(x, y, z) mat.color = vec3(x, y, z) / 100.0;

#define ROUGH(x) mat.roughness = float(x);

#define END_MAT() }

// clang-format on

#endif // MARKDOWN_GLSL_INCLUDED
