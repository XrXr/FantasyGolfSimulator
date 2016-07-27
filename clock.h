#pragma once

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

bool elapsed_ns(uint64_t* time) {
    struct timespec tp;
    int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (ret != 0) return false;
    *time = tp.tv_sec * 1000000000 + tp.tv_nsec;
    return true;
}
