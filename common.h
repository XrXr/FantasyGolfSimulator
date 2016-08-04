#pragma once

#include <limits.h>

typedef struct {
    float x;
    float y;
    float z;
} vec3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4;

typedef struct {
    float value[16];
} mat4;

#define GOLF_RESTART_IDX USHRT_MAX