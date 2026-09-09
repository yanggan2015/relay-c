#include "util.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

int relay_detect_lan_ip(char *buf, size_t cap) {
#ifdef _WIN32
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in remote, local;
    int local_len = (int)sizeof(local);
    if (!buf || cap == 0) return -1;
    buf[0] = '\0';
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(s, (struct sockaddr *)&remote, sizeof(remote)) == 0 &&
        getsockname(s, (struct sockaddr *)&local, &local_len) == 0)
        inet_ntop(AF_INET, &local.sin_addr, buf, (socklen_t)cap);
    closesocket(s);
    WSACleanup();
#else
    int fd;
    struct sockaddr_in remote, local;
    socklen_t local_len = sizeof(local);
    if (!buf || cap == 0) return -1;
    buf[0] = '\0';
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) == 0 &&
        getsockname(fd, (struct sockaddr *)&local, &local_len) == 0)
        inet_ntop(AF_INET, &local.sin_addr, buf, cap);
    close(fd);
#endif
    return buf[0] ? 0 : -1;
}

static int relay_path_is_abs(const char *path) {
    if (!path || !path[0]) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':')
        return 1;
    return 0;
}

void relay_path_join(char *dst, size_t cap, const char *a, const char *b) {
    size_t la;
    int need_sep;
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (b && relay_path_is_abs(b)) {
        relay_str_copy(dst, cap, b);
        return;
    }
    if (!a || !a[0]) {
        relay_str_copy(dst, cap, b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        relay_str_copy(dst, cap, a);
        return;
    }
    la = strlen(a);
    need_sep = (a[la - 1] != '/' && a[la - 1] != '\\');
#ifdef _WIN32
    snprintf(dst, cap, "%s%s%s", a, need_sep ? "\\" : "", b);
#else
    snprintf(dst, cap, "%s%s%s", a, need_sep ? "/" : "", b);
#endif
}

void relay_dirname(char *dst, size_t cap, const char *path) {
    char tmp[1024];
    char *p;
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!path || !path[0]) return;
    relay_str_copy(tmp, sizeof(tmp), path);
    p = tmp + strlen(tmp);
    while (p > tmp && (p[-1] == '/' || p[-1] == '\\')) {
        --p;
        *p = '\0';
    }
    while (p > tmp && p[-1] != '/' && p[-1] != '\\') --p;
    if (p <= tmp) {
        relay_str_copy(dst, cap, ".");
        return;
    }
    if (p == tmp + 1 && (tmp[0] == '/' || tmp[0] == '\\')) {
        dst[0] = tmp[0];
        dst[1] = '\0';
        return;
    }
    *p = '\0';
    relay_str_copy(dst, cap, tmp);
}

void relay_abspath(char *dst, size_t cap, const char *path) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!path) return;
#ifdef _WIN32
    if (!_fullpath(dst, path, (unsigned)cap))
        relay_str_copy(dst, cap, path);
#else
    {
        char resolved[PATH_MAX];
        if (realpath(path, resolved)) {
            relay_str_copy(dst, cap, resolved);
        } else if (relay_path_is_abs(path)) {
            relay_str_copy(dst, cap, path);
        } else {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)))
                relay_path_join(dst, cap, cwd, path);
            else
                relay_str_copy(dst, cap, path);
        }
    }
#endif
}

int relay_file_exists(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0;
}

int relay_exe_dir(char *dst, size_t cap) {
    if (!dst || cap == 0) return -1;
    dst[0] = '\0';
#ifdef _WIN32
    {
        wchar_t wpath[MAX_PATH];
        char path[1024];
        DWORD n = GetModuleFileNameW(NULL, wpath, MAX_PATH);
        int m;
        if (n == 0 || n >= MAX_PATH) return -1;
        m = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, (int)sizeof(path), NULL, NULL);
        if (m <= 0) return -1;
        relay_dirname(dst, cap, path);
        return dst[0] ? 0 : -1;
    }
#else
    {
        char link[1024];
        ssize_t n = readlink("/proc/self/exe", link, sizeof(link) - 1);
        if (n <= 0) return -1;
        link[n] = '\0';
        relay_dirname(dst, cap, link);
        return dst[0] ? 0 : -1;
    }
#endif
}

int relay_cwd(char *dst, size_t cap) {
    if (!dst || cap == 0) return -1;
    dst[0] = '\0';
#ifdef _WIN32
    {
        DWORD n = GetCurrentDirectoryA((DWORD)cap, dst);
        if (n == 0 || n >= cap) {
            dst[0] = '\0';
            return -1;
        }
        return 0;
    }
#else
    if (!getcwd(dst, cap)) {
        dst[0] = '\0';
        return -1;
    }
    return 0;
#endif
}

int relay_home_dir(char *dst, size_t cap) {
    if (!dst || cap == 0) return -1;
    dst[0] = '\0';
#ifdef _WIN32
    {
        const char *home = getenv("USERPROFILE");
        if (!home || !home[0]) {
            const char *hd = getenv("HOMEDRIVE");
            const char *hp = getenv("HOMEPATH");
            if (hd && hp) {
                snprintf(dst, cap, "%s%s", hd, hp);
                return dst[0] ? 0 : -1;
            }
            return -1;
        }
        relay_str_copy(dst, cap, home);
        return 0;
    }
#else
    {
        const char *home = getenv("HOME");
        if (!home || !home[0]) return -1;
        relay_str_copy(dst, cap, home);
        return 0;
    }
#endif
}

int relay_user_config_dir(char *dst, size_t cap) {
    char home[1024];
    if (relay_home_dir(home, sizeof(home)) != 0) return -1;
    relay_path_join(dst, cap, home, ".relay-c");
    return dst[0] ? 0 : -1;
}

#define EADK_TOOL_ID "relay"
#define EADK_CONFIG_PREFERRED "relay_config.json"

static int relay_config_accepts_tool(const char *path, const char *expect_tool) {
    size_t n = 0;
    char *txt;
    cJSON *root, *eadk, *tool;
    int ok = 1;
    if (!path || !path[0] || !expect_tool || !expect_tool[0]) return -1;
    txt = relay_read_file(path, &n);
    if (!txt) return -1;
    root = cJSON_Parse(txt);
    free(txt);
    if (!root) return -1;
    eadk = cJSON_GetObjectItemCaseSensitive(root, "eadk");
    if (cJSON_IsObject(eadk)) {
        tool = cJSON_GetObjectItemCaseSensitive(eadk, "tool");
        if (cJSON_IsString(tool) && tool->valuestring && tool->valuestring[0]) {
            if (strcmp(tool->valuestring, expect_tool) != 0) ok = 0;
        }
    }
    cJSON_Delete(root);
    return ok;
}

void relay_json_ensure_eadk_tool(cJSON *root, const char *tool_id) {
    cJSON *eadk;
    if (!root || !tool_id || !tool_id[0]) return;
    eadk = cJSON_GetObjectItemCaseSensitive(root, "eadk");
    if (!cJSON_IsObject(eadk)) {
        eadk = cJSON_CreateObject();
        if (!eadk) return;
        cJSON_AddItemToObject(root, "eadk", eadk);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(eadk, "tool");
    cJSON_AddStringToObject(eadk, "tool", tool_id);
}

static int relay_try_config_candidate(char *dst, size_t cap, const char *dir, const char *name) {
    char cand[1024];
    if (!dir || !dir[0] || !name || !name[0]) return -1;
    relay_path_join(cand, sizeof(cand), dir, name);
    if (!relay_file_exists(cand)) return -1;
    if (relay_config_accepts_tool(cand, EADK_TOOL_ID) != 1) {
        relay_log("[config] skip %s (eadk.tool mismatch or unreadable)", cand);
        return -1;
    }
    relay_abspath(dst, cap, cand);
    return dst[0] ? 0 : -1;
}

int relay_resolve_config_path(char *dst, size_t cap, const char *explicit_path) {
    char cwd[1024], exe_dir[1024], user_dir[1024];
    static const char *legacy[] = {"boards.json", "config.json", NULL};
    int i;
    if (!dst || cap == 0) return -1;
    dst[0] = '\0';

    if (explicit_path && explicit_path[0]) {
        if (!relay_file_exists(explicit_path)) return -1;
        if (relay_config_accepts_tool(explicit_path, EADK_TOOL_ID) != 1) {
            relay_log("[config] reject %s: eadk.tool is not %s", explicit_path, EADK_TOOL_ID);
            return -1;
        }
        relay_abspath(dst, cap, explicit_path);
        return dst[0] ? 0 : -1;
    }

    if (relay_cwd(cwd, sizeof(cwd)) == 0) {
        if (relay_try_config_candidate(dst, cap, cwd, EADK_CONFIG_PREFERRED) == 0) return 0;
    }
    if (relay_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        if (relay_try_config_candidate(dst, cap, exe_dir, EADK_CONFIG_PREFERRED) == 0) return 0;
    }
    if (relay_user_config_dir(user_dir, sizeof(user_dir)) == 0) {
        if (relay_try_config_candidate(dst, cap, user_dir, EADK_CONFIG_PREFERRED) == 0) return 0;
    }
    if (relay_cwd(cwd, sizeof(cwd)) == 0) {
        for (i = 0; legacy[i]; ++i)
            if (relay_try_config_candidate(dst, cap, cwd, legacy[i]) == 0) return 0;
    }
    if (relay_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        for (i = 0; legacy[i]; ++i)
            if (relay_try_config_candidate(dst, cap, exe_dir, legacy[i]) == 0) return 0;
    }
    if (relay_user_config_dir(user_dir, sizeof(user_dir)) == 0) {
        for (i = 0; legacy[i]; ++i)
            if (relay_try_config_candidate(dst, cap, user_dir, legacy[i]) == 0) return 0;
    }
    return -1;
}

static const char *k_default_empty_config =
    "{\"eadk\":{\"tool\":\"relay\"},"
    "\"server\":{\"port\":18053,\"discoverable\":false,\"display_name\":\"relay\"},"
    "\"relays\":{},\"boards\":{}}\n";

int relay_resolve_or_create_config_path(char *dst, size_t cap, const char *explicit_path) {
    char dir[1024], path[1024];
    if (relay_resolve_config_path(dst, cap, explicit_path) == 0) return 0;
    if (explicit_path && explicit_path[0]) return -1;

    if (relay_exe_dir(dir, sizeof(dir)) != 0) {
        if (relay_cwd(dir, sizeof(dir)) != 0) return -1;
    }
    relay_path_join(path, sizeof(path), dir, EADK_CONFIG_PREFERRED);
    if (relay_file_exists(path) && relay_config_accepts_tool(path, EADK_TOOL_ID) == 1) {
        relay_abspath(dst, cap, path);
        return dst[0] ? 0 : -1;
    }
    if (relay_write_file(path, k_default_empty_config, strlen(k_default_empty_config)) != 0) {
        relay_log("[config] failed to create default config: %s", path);
        return -1;
    }
    relay_abspath(dst, cap, path);
    if (!dst[0]) return -1;
    relay_log("[config] created empty config: %s", dst);
    return 0;
}
