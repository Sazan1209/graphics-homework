#include "tonemap.h"

const float minEyeLum = 0.0001;

const float minScreenLum = 1.0 / 255.0;
const float minScreenB = log(minScreenLum);
const float maxScreenLum = 1.0;
const float maxScreenB = log(maxScreenLum);

const vec3 lumVec = vec3(0.2126, 0.7152, 0.0722);
