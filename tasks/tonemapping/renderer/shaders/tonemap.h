#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(tonemap_const)

#define BUCKET_COUNT 128

const shader_uint bucketCount = BUCKET_COUNT;
const shader_float minLum = 0.0001;
const shader_float minB = log(minLum);
const shader_float maxLum = 1.1;
const shader_float maxB = log(maxLum);
const shader_float dB = (maxB - minB) / bucketCount;
const shader_vec3 lumVec = shader_vec3(0.2126, 0.7152, 0.0722);

SHADER_NAMESPACE_END
