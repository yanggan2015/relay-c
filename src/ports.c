#include "relay_ports.h"
#include "relay_types.h"
#include "util.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELAY_PORTS_MAX 64

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <glob.h>
#include <sys/stat.h>
#endif

static int port_exists(const char ports[][RELAY_MAX_PORT], int n, const char *name) {
    int i;
    for (i = 0; i < n; ++i)
        if (strcmp(ports[i], name) == 0) return 1;
    return 0;
}

static void port_add(char ports[][RELAY_MAX_PORT], int max, int *n, const char *name) {
    if (!name || !name[0] || *n >= max || port_exists(ports, *n, name)) return;
    relay_str_copy(ports[*n], RELAY_MAX_PORT, name);
    (*n)++;
}

static int win_com_num(const char *s) {
    if (!s || strncmp(s, "COM", 3) != 0) return 99999;
    return atoi(s + 3);
}

static int win_port_cmp(const void *a, const void *b) {
    int da = win_com_num((const char *)a);
    int db = win_com_num((const char *)b);
    if (da != db) return da - db;
    return strcmp((const char *)a, (const char *)b);
}

#ifdef _WIN32
static void enum_win_registry(char ports[][RELAY_MAX_PORT], int max, int *n) {
    HKEY hkey;
    DWORD idx = 0;
    char value_name[256];
    char data[RELAY_MAX_PORT];
    DWORD value_name_size, data_size, type;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return;

    for (;;) {
        value_name_size = (DWORD)sizeof(value_name);
        data_size = (DWORD)sizeof(data);
        if (RegEnumValueA(hkey, idx++, value_name, &value_name_size,
                          NULL, &type, (LPBYTE)data, &data_size) != ERROR_SUCCESS)
            break;
        if (type == REG_SZ)
            port_add(ports, max, n, data);
    }
    RegCloseKey(hkey);
}

static void enum_win_querydos(char ports[][RELAY_MAX_PORT], int max, int *n) {
    int i;
    for (i = 1; i <= 256; ++i) {
        char name[16];
        char target[512];
        snprintf(name, sizeof(name), "COM%d", i);
        if (QueryDosDeviceA(name, target, (DWORD)sizeof(target)) > 0)
            port_add(ports, max, n, name);
    }
}

static int enum_windows_ports(char ports[][RELAY_MAX_PORT], int max) {
    int n = 0;
    enum_win_registry(ports, max, &n);
    enum_win_querydos(ports, max, &n);
    if (n > 1)
        qsort(ports, (size_t)n, RELAY_MAX_PORT, win_port_cmp);
    return n;
}
#else
static int str_port_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void enum_linux_glob(const char *pattern, char ports[][RELAY_MAX_PORT], int max, int *n) {
    glob_t g;
    size_t i;
    if (glob(pattern, 0, NULL, &g) != 0) return;
    for (i = 0; i < g.gl_pathc; ++i)
        port_add(ports, max, n, g.gl_pathv[i]);
    globfree(&g);
}

static int enum_linux_ports(char ports[][RELAY_MAX_PORT], int max) {
    int n = 0;
    enum_linux_glob("/dev/ttyUSB*", ports, max, &n);
    enum_linux_glob("/dev/ttyACM*", ports, max, &n);
    enum_linux_glob("/dev/ttyS*", ports, max, &n);
    if (n > 1)
        qsort(ports, (size_t)n, RELAY_MAX_PORT, str_port_cmp);
    return n;
}
#endif

static cJSON *ports_to_json_array(char ports[][RELAY_MAX_PORT], int n) {
    cJSON *arr = cJSON_CreateArray();
    int i;
    for (i = 0; i < n; ++i)
        cJSON_AddItemToArray(arr, cJSON_CreateString(ports[i]));
    return arr;
}

cJSON *relay_ports_enumerate_json(void) {
    cJSON *root = cJSON_CreateObject();
    char ports[RELAY_PORTS_MAX][RELAY_MAX_PORT];
    int n;

#ifdef _WIN32
    n = enum_windows_ports(ports, RELAY_PORTS_MAX);
    cJSON_AddStringToObject(root, "platform", "windows");
    cJSON_AddItemToObject(root, "windows_ports", ports_to_json_array(ports, n));
    cJSON_AddItemToObject(root, "linux_ports", cJSON_CreateArray());
#else
    n = enum_linux_ports(ports, RELAY_PORTS_MAX);
    cJSON_AddStringToObject(root, "platform", "linux");
    cJSON_AddItemToObject(root, "linux_ports", ports_to_json_array(ports, n));
    cJSON_AddItemToObject(root, "windows_ports", cJSON_CreateArray());
#endif
    return root;
}
