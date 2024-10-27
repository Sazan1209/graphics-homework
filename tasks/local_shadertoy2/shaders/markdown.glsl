#ifndef MARKDOWN_GLSL_INCLUDED
#define MARKDOWN_GLSL_INCLUDED

#extension GL_ARB_separate_shader_objects : enable

// clang-format off

#define BEGIN_GROUP() \
{\
    mat4 tf0 = mat4(1);

#define BEGIN_FIG() \
{\
    mat4 tf0 = tf0;

#define F_YAW(angle) \
    tf0 = mat4(transpose(yaw(angle))) * tf0;

#define F_PITCH(angle) \
    tf0 = mat4(transpose(pitch(angle))) * tf0;

#define F_ROLL(angle) \
    tf0 = mat4(transpose(roll(angle))) * tf0;

#define F_SHIFT(x, y, z)\
    tf0[3] -= vec4(x, y, z, 0);

#define END_FIG(sdF, param)\
    float tmp = sdF(vec3(tf0 * vec4(p, 1)), param);\
    if (tmp < l){\
        l = tmp;\
        mat_num = 0;\
        tf = tf0;\
    }\
}

#define END_FIG_MAT(sdF, param, num)\
    float tmp = sdF(vec3(tf0 * vec4(p, 1)), param);\
    if (tmp < l){\
        l = tmp;\
        mat_num = num;\
        tf = tf0;\
    }\
}

#define END_GROUP()\
}

#define BEGIN_MAT(num)\
    if (mat_num == num){

#define COLOR(x, y, z)\
        color *= vec3(x, y, z) / 100.0;

#define SHINE(x)\
        shine = float(x);

#define TEXTURE(textureF)\
        color *= textureF(p_rel, normal_rel).xyz;

#define END_MAT()\
        color *= calculate_light(p, shine, normal);\
        return vec4(color, 1);\
    }

// clang-format on

#endif // MARKDOWN_GLSL_INCLUDED
