/*
 * LAN UDP discovery responder (EADK1 @ 18071).
 * Replies only when boards.json server.discoverable is true (re-read each query).
 */
#include "eadk_discovery.h"
#include "util.h"
#include "relay_types.h"

#include <cjson/cJSON.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET disc_sock_t;
#define DISC_INVALID INVALID_SOCKET
#define disc_close   closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int disc_sock_t;
#define DISC_INVALID (-1)
#define disc_close   close
#endif

#ifndef CFG_PATH
#define CFG_PATH RELAY_MAX_PATH
#endif

struct EadkDiscovery {
    volatile int stop;
    pthread_t thr;
    int thr_ok;
    disc_sock_t sock;
    pthread_mutex_t mu;
    char config_path[CFG_PATH];
    int http_port;
    char tool[32];
    char version[64];
};

static int is_discover_query(const char *buf, int n) {
    if (!buf || n < (int)sizeof(EADK_DISCOVERY_MAGIC)) return 0;
    if (strncmp(buf, EADK_DISCOVERY_MAGIC, sizeof(EADK_DISCOVERY_MAGIC) - 1) != 0)
        return 0;
    {
        const char *p = buf + (sizeof(EADK_DISCOVERY_MAGIC) - 1);
        while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') p++;
        if (*p == 'q' || *p == 'Q') return 1;
    }
    if (strstr(buf, "\"cmd\":\"q\"") || strstr(buf, "\"cmd\":\"discover\"")) return 1;
    if (strstr(buf, "\"t\":\"q\"") || strstr(buf, "\"t\":\"discover\"")) return 1;
    return 0;
}

static int json_truthy(cJSON *v, int fallback) {
    if (!v) return fallback;
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v) ? 1 : 0;
    if (cJSON_IsNumber(v)) return v->valuedouble != 0;
    if (cJSON_IsString(v) && v->valuestring)
        return relay_parse_bool(v->valuestring, fallback);
    return fallback;
}

/* Returns 1 if discoverable; fills display_name. */
static int read_server_discover(const char *config_path, char *display_name, size_t ncap) {
    char *txt;
    cJSON *root, *server;
    int disc = 0;
    if (display_name && ncap) display_name[0] = '\0';
    if (!config_path || !config_path[0]) return 0;
    txt = relay_read_file(config_path, NULL);
    if (!txt) return 0;
    root = cJSON_Parse(txt);
    free(txt);
    if (!root) return 0;
    server = cJSON_GetObjectItemCaseSensitive(root, "server");
    if (cJSON_IsObject(server)) {
        disc = json_truthy(cJSON_GetObjectItemCaseSensitive(server, "discoverable"), 1);
        {
            cJSON *dn = cJSON_GetObjectItemCaseSensitive(server, "display_name");
            if (cJSON_IsString(dn) && dn->valuestring && display_name && ncap)
                relay_str_copy(display_name, ncap, dn->valuestring);
        }
    }
    cJSON_Delete(root);
    return disc;
}

static char *build_reply_json(EadkDiscovery *d, const char *config_path, int http_port) {
    char host[64];
    char display_name[128];
    cJSON *root, *caps;
    char *json;

    if (!read_server_discover(config_path, display_name, sizeof(display_name)))
        return NULL;

    host[0] = '\0';
    if (relay_detect_lan_ip(host, sizeof(host)) != 0 || !host[0])
        relay_str_copy(host, sizeof(host), "127.0.0.1");

    root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddNumberToObject(root, "v", 1);
    cJSON_AddStringToObject(root, "cmd", "a");
    cJSON_AddStringToObject(root, "magic", EADK_DISCOVERY_MAGIC);
    cJSON_AddStringToObject(root, "tool", d->tool[0] ? d->tool : "relay");
    cJSON_AddStringToObject(root, "product", "EADK Relay");
    cJSON_AddStringToObject(root, "tool_version", "1");
    cJSON_AddNumberToObject(root, "http_port", http_port);
    cJSON_AddStringToObject(root, "host", host);
    cJSON_AddStringToObject(root, "display_name", display_name);
    cJSON_AddStringToObject(root, "version", d->version[0] ? d->version : RELAY_VERSION);
    cJSON_AddStringToObject(root, "open_path", "/");
    cJSON_AddStringToObject(root, "docs_path", "/docs");
    cJSON_AddStringToObject(root, "health_path", "/api/health");
    cJSON_AddStringToObject(root, "api_help_path", "/api/help");
    caps = cJSON_AddArrayToObject(root, "capabilities");
    if (caps) {
        cJSON_AddItemToArray(caps, cJSON_CreateString("http"));
        cJSON_AddItemToArray(caps, cJSON_CreateString("webui"));
        cJSON_AddItemToArray(caps, cJSON_CreateString("api"));
    }
    cJSON_AddBoolToObject(root, "discoverable", 1);

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static void *discovery_thread(void *arg) {
    EadkDiscovery *d = (EadkDiscovery *)arg;
    char buf[2048];
    while (!d->stop) {
        struct sockaddr_in from;
        socklen_t fromlen = (socklen_t)sizeof(from);
        fd_set rfds;
        struct timeval tv;
        int sel, n;
        char path[CFG_PATH];
        int http_port;
        char *json;
        char packet[4096];
        int plen;

        FD_ZERO(&rfds);
        FD_SET(d->sock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 300000;
#ifdef _WIN32
        sel = select(0, &rfds, NULL, NULL, &tv);
#else
        sel = select((int)d->sock + 1, &rfds, NULL, NULL, &tv);
#endif
        if (sel <= 0) continue;
        n = (int)recvfrom(d->sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
        if (n <= 0) continue;
        buf[n] = '\0';
        if (!is_discover_query(buf, n)) continue;

        pthread_mutex_lock(&d->mu);
        strncpy(path, d->config_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        http_port = d->http_port;
        pthread_mutex_unlock(&d->mu);

        json = build_reply_json(d, path, http_port);
        if (!json) continue; /* discoverable false → silent */

        plen = snprintf(packet, sizeof(packet), "%s\n%s", EADK_DISCOVERY_MAGIC, json);
        free(json);
        if (plen <= 0 || plen >= (int)sizeof(packet)) continue;
        sendto(d->sock, packet, (size_t)plen, 0, (struct sockaddr *)&from, fromlen);
    }
    return NULL;
}

EadkDiscovery *eadk_discovery_start(const char *config_path, int http_port,
                                    const char *tool, const char *version) {
    EadkDiscovery *d;
    int yes = 1;
    struct sockaddr_in addr;

#ifdef _WIN32
    {
        static int wsa_once = 0;
        if (!wsa_once) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                relay_log("[discovery] WSAStartup failed");
                return NULL;
            }
            wsa_once = 1;
        }
    }
#endif

    d = (EadkDiscovery *)calloc(1, sizeof(*d));
    if (!d) return NULL;
    pthread_mutex_init(&d->mu, NULL);
    if (config_path) {
        strncpy(d->config_path, config_path, sizeof(d->config_path) - 1);
        d->config_path[sizeof(d->config_path) - 1] = '\0';
    }
    d->http_port = http_port;
    relay_str_copy(d->tool, sizeof(d->tool), tool ? tool : "relay");
    relay_str_copy(d->version, sizeof(d->version), version ? version : RELAY_VERSION);
    d->sock = DISC_INVALID;
    d->stop = 0;

    d->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (d->sock == DISC_INVALID) {
        relay_log("[discovery] socket failed");
        eadk_discovery_stop(d);
        return NULL;
    }
    setsockopt(d->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    setsockopt(d->sock, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)EADK_DISCOVERY_UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(d->sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        relay_log("[discovery] bind UDP %d failed (another instance?)", EADK_DISCOVERY_UDP_PORT);
        eadk_discovery_stop(d);
        return NULL;
    }

    if (pthread_create(&d->thr, NULL, discovery_thread, d) != 0) {
        relay_log("[discovery] thread create failed");
        eadk_discovery_stop(d);
        return NULL;
    }
    d->thr_ok = 1;
    relay_log("[discovery] listening UDP %d (EADK1, reply if discoverable)", EADK_DISCOVERY_UDP_PORT);
    return d;
}

void eadk_discovery_set(EadkDiscovery *d, const char *config_path, int http_port) {
    if (!d) return;
    pthread_mutex_lock(&d->mu);
    if (config_path) {
        strncpy(d->config_path, config_path, sizeof(d->config_path) - 1);
        d->config_path[sizeof(d->config_path) - 1] = '\0';
    } else {
        d->config_path[0] = '\0';
    }
    d->http_port = http_port;
    pthread_mutex_unlock(&d->mu);
}

void eadk_discovery_stop(EadkDiscovery *d) {
    if (!d) return;
    d->stop = 1;
    if (d->thr_ok) {
        pthread_join(d->thr, NULL);
        d->thr_ok = 0;
    }
    if (d->sock != DISC_INVALID) {
        disc_close(d->sock);
        d->sock = DISC_INVALID;
    }
    pthread_mutex_destroy(&d->mu);
    free(d);
}
