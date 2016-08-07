#include "clock.h"
#ifdef _WIN32

#include <Windows.h>
ULONGLONG WINAPI GetTickCount64(void);

static double performance_freq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return 1000000000.0 / freq.QuadPart;
}

bool elapsed_ns(uint64_t* time) {
    static double count_to_ns = 0;
    if (!count_to_ns) count_to_ns = performance_freq();

    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
	*time = count.QuadPart * count_to_ns;
    return true;
}

#else

#include <time.h>

bool elapsed_ns(uint64_t* time) {
    struct timespec tp;
    int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (ret != 0) return false;
    *time = tp.tv_sec * 1000000000 + tp.tv_nsec;
    return true;
}

#endif