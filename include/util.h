#ifndef RELAY_UTIL_H
#define RELAY_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON cJSON;

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

void relay_abspath(char *dst, size_t cap, const char *path);
void relay_dirname(char *dst, size_t cap, const char *path);
void relay_path_join(char *dst, size_t cap, const char *a, const char *b);
int relay_file_exists(const char *path);
int relay_exe_dir(char *dst, size_t cap);
int relay_cwd(char *dst, size_t cap);
int relay_home_dir(char *dst, size_t cap);
/* <home>/.relay-c */
int relay_user_config_dir(char *dst, size_t cap);
/*
 * EADK flat-kit config resolution (tool id = "relay"):
 *   Preferred: relay_config.json
 *   Legacy:    boards.json, then config.json
 * Order (no -c): preferred in cwd/exe/home, then legacies in cwd/exe/home.
 * Auto-create writes preferred under exe (else cwd) with eadk.tool set.
 */
int relay_resolve_config_path(char *dst, size_t cap, const char *explicit_path);
int relay_resolve_or_create_config_path(char *dst, size_t cap, const char *explicit_path);
/* Ensure root JSON has {"eadk":{"tool":"relay"}} before save. */
void relay_json_ensure_eadk_tool(cJSON *root, const char *tool_id);

#ifdef __cplusplus
}
#endif

#endif
