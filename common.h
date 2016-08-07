#pragma once

#include <limits.h>
#include <stdio.h>

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
#define print_vec3(V)  printf(#V": x=%f y=%f z=%f\n", V.x, V.y, V.z)
#define min(X, Y)  (X < Y ? X : Y)
#define deg_to_rad(X) (X * (M_PI / 180))
