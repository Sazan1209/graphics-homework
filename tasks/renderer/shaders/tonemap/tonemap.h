#ifndef TONEMAP_H
#define TONEMAP_H

#include "cpp_glsl_compat.h"

SHADER_NAMESPACE(tonemap_const)

#define BUCKET_COUNT 128

const shader_uint bucketCount = BUCKET_COUNT;

SHADER_NAMESPACE_END

#endif // TONEMAP_H
