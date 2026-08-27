#include "relay_config.h"
#include "relay_http.h"
#include "relay_lock.h"
#include "relay_serial.h"
#include "relay_startup.h"
#include "relay_types.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <signal.h>
#endif

static volatile int g_running = 1;

static void on_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-c boards.json] [-p port] [-n] [-h]\n", prog);
    fprintf(stderr, "  relay-c v%s - relay/board control server\n", RELAY_VERSION);
}

int main(int argc, char **argv) {
    AppConfig cfg;
    RelayStateCache cache;
    RelayHttpServer *srv;
    RelayService *service;
    const char *config_path = "boards.json";
    int port = 18053;
    int dry_run = 0;
    char verr[256];
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
            relay_executor_set_dry_run(1);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

#ifdef _WIN32
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#endif

    relay_lock_init();
    memset(&cfg, 0, sizeof(cfg));
    memset(&cache, 0, sizeof(cache));
    for (i = 0; i < RELAY_MAX_RELAYS; ++i) {
        int ch;
        for (ch = 0; ch <= RELAY_MAX_CHANNELS; ++ch)
            cache.state[i][ch] = -1;
    }

    if (relay_config_load(&cfg, config_path) != 0) {
        relay_log("[relay-c] warning: cannot load %s, starting with empty config", config_path);
        relay_str_copy(cfg.config_path, sizeof(cfg.config_path), config_path);
        relay_str_copy(cfg.platform, sizeof(cfg.platform), relay_detect_platform());
    } else if (relay_config_validate_all(&cfg, verr, sizeof(verr)) != 0) {
        relay_log("[relay-c] config validation warning: %s", verr);
    }

    if (cfg.server_port_set) port = cfg.server_port;

    relay_log("[relay-c] v%s platform=%s relays=%d boards=%d port=%d dry_run=%d",
              RELAY_VERSION, cfg.platform, cfg.n_relays, cfg.n_boards, port, dry_run);

    srv = relay_http_create(&cfg, &cache);
    if (!srv) {
        relay_log("[relay-c] failed to create HTTP server");
        relay_lock_shutdown();
        return 1;
    }

    service = relay_http_service(srv);
    if (service) relay_startup_init_relays(service, &cfg, &cache);

    if (relay_http_start(srv, port) != 0) {
        relay_log("[relay-c] failed to bind port %d", port);
        relay_http_free(srv);
        relay_lock_shutdown();
        return 1;
    }

    while (g_running) {
        relay_http_poll(srv, 200);
    }

    relay_http_free(srv);
    relay_lock_shutdown();
    relay_log("[relay-c] stopped");
    return 0;
}
