#ifndef RELAY_UTIL_H
#define RELAY_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void relay_sleep_ms(unsigned int ms);
double relay_now_seconds(void);

void relay_str_copy(char *dst, size_t cap, const char *src);
int  relay_str_eq_ci(const char *a, const char *b);
int  relay_parse_bool(const char *s, int fallback);
int  relay_parse_int(const char *s, int fallback);
double relay_parse_float(const char *s, double fallback);

char *relay_read_file(const char *path, size_t *out_len);
int   relay_write_file(const char *path, const char *data, size_t len);

void relay_log(const char *fmt, ...);

const char *relay_detect_platform(void);
const char *relay_resolved_port(const char *port, const char *linux_port,
                                 const char *windows_port, const char *platform);
int relay_detect_lan_ip(char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
