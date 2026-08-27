#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

void relay_sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

double relay_now_seconds(void) {
#ifdef _WIN32
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

void relay_str_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

int relay_str_eq_ci(const char *a, const char *b) {
    if (!a || !b) return 0;
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

int relay_parse_bool(const char *s, int fallback) {
    if (!s || !s[0]) return fallback;
    if (relay_str_eq_ci(s, "1") || relay_str_eq_ci(s, "true") ||
        relay_str_eq_ci(s, "yes") || relay_str_eq_ci(s, "on"))
        return 1;
    if (relay_str_eq_ci(s, "0") || relay_str_eq_ci(s, "false") ||
        relay_str_eq_ci(s, "no") || relay_str_eq_ci(s, "off"))
        return 0;
    return fallback;
}

int relay_parse_int(const char *s, int fallback) {
    char *end = NULL;
    long v;
    if (!s || !s[0]) return fallback;
    v = strtol(s, &end, 10);
    if (end == s) return fallback;
    return (int)v;
}

double relay_parse_float(const char *s, double fallback) {
    char *end = NULL;
    double v;
    if (!s || !s[0]) return fallback;
    v = strtod(s, &end);
    if (end == s) return fallback;
    return v;
}

char *relay_read_file(const char *path, size_t *out_len) {
    FILE *f;
    char *buf;
    long sz;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

int relay_write_file(const char *path, const char *data, size_t len) {
    FILE *f;
    if (!path || !data) return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

void relay_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

const char *relay_detect_platform(void) {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

const char *relay_resolved_port(const char *port, const char *linux_port,
                                 const char *windows_port, const char *platform) {
    if (platform && relay_str_eq_ci(platform, "windows")) {
        if (windows_port && windows_port[0]) return windows_port;
    } else {
        if (linux_port && linux_port[0]) return linux_port;
    }
    if (port && port[0]) return port;
    if (windows_port && windows_port[0]) return windows_port;
    if (linux_port && linux_port[0]) return linux_port;
    return "";
}
