#include "relay_config.h"
#include "relay_http.h"
#include "relay_lock.h"
#include "relay_serial.h"
#include "relay_startup.h"
#include "relay_types.h"
#include "eadk_discovery.h"
#include "cli_http.h"
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
    fprintf(stderr,
            "relay-c v%s — relay/board control\n\n"
            "Usage:\n"
            "  %s [-c boards.json] [-p port] [-n]     start server (default)\n"
            "  %s help | -h\n"
            "  %s version | -v\n"
            "  %s status [-c boards.json] [-p port]\n"
            "  %s call <path_and_query> [-c] [-p]\n"
            "  %s board-action <board> <action> [-c] [-p]\n"
            "  %s relay-set <relay> <channel> <0|1> [-c] [-p]\n\n"
            "CLI write ops use local HTTP (server must be running).\n",
            RELAY_VERSION, prog, prog, prog, prog, prog, prog, prog);
}

static int parse_common_flags(int argc, char **argv, int start,
                              const char **config_path, int *port) {
    int i;
    for (i = start; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            *config_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            *port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            /* ignore for CLI helpers */
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

static int resolve_port(const char *config_path, int port_flag) {
    AppConfig cfg;
    if (port_flag > 0) return port_flag;
    memset(&cfg, 0, sizeof(cfg));
    if (relay_config_load(&cfg, config_path) == 0 && cfg.server_port_set)
        return cfg.server_port;
    return 18053;
}

static int cli_print_http(int port, const char *path) {
    int status = 0;
    char *body = relay_cli_http_get(port, path, &status);
    if (!body) {
        fprintf(stderr, "error: cannot reach http://127.0.0.1:%d%s\n", port, path);
        return 1;
    }
    fputs(body, stdout);
    if (body[0] && body[strlen(body) - 1] != '\n') fputc('\n', stdout);
    free(body);
    return (status >= 200 && status < 400) ? 0 : 1;
}

static int cmd_status(const char *config_path, int port_flag) {
    int port = resolve_port(config_path, port_flag);
    int status = 0;
    char *body = relay_cli_http_get(port, "/api/health", &status);
    if (body) {
        fputs(body, stdout);
        if (body[0] && body[strlen(body) - 1] != '\n') fputc('\n', stdout);
        free(body);
        return (status >= 200 && status < 400) ? 0 : 1;
    }
    {
        AppConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        fprintf(stderr, "server not reachable on port %d — showing config\n", port);
        if (relay_config_load(&cfg, config_path) != 0) {
            fprintf(stderr, "cannot load %s\n", config_path);
            return 1;
        }
        printf("config_path=%s\n", cfg.config_path);
        printf("server_port=%d\n", cfg.server_port_set ? cfg.server_port : 18053);
        printf("discoverable=%d\n", cfg.discoverable);
        printf("display_name=%s\n", cfg.display_name);
        printf("relays=%d\n", cfg.n_relays);
        printf("boards=%d\n", cfg.n_boards);
        printf("platform=%s\n", cfg.platform);
        return 0;
    }
}

static int run_serve(int argc, char **argv) {
    AppConfig cfg;
    RelayStateCache cache;
    RelayHttpServer *srv;
    RelayService *service;
    EadkDiscovery *disc = NULL;
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
        } else if (strcmp(argv[i], "serve") == 0) {
            /* explicit serve */
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
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

    disc = eadk_discovery_start(cfg.config_path, port, "relay", RELAY_VERSION);

    while (g_running) {
        relay_http_poll(srv, 200);
    }

    eadk_discovery_stop(disc);
    relay_http_free(srv);
    relay_lock_shutdown();
    relay_log("[relay-c] stopped");
    return 0;
}

int main(int argc, char **argv) {
    const char *config_path = "boards.json";
    int port_flag = 0;
    const char *cmd;

    if (argc < 2)
        return run_serve(argc, argv);

    cmd = argv[1];
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("relay-c %s\n", RELAY_VERSION);
        return 0;
    }

    /* Flags-only / serve → current server behavior */
    if (cmd[0] == '-' || strcmp(cmd, "serve") == 0)
        return run_serve(argc, argv);

    if (strcmp(cmd, "status") == 0) {
        if (parse_common_flags(argc, argv, 2, &config_path, &port_flag) != 0) return 1;
        return cmd_status(config_path, port_flag);
    }
    if (strcmp(cmd, "call") == 0) {
        const char *path;
        int port;
        if (argc < 3) {
            fprintf(stderr, "usage: %s call <path_and_query> [-c] [-p]\n", argv[0]);
            return 1;
        }
        path = argv[2];
        if (parse_common_flags(argc, argv, 3, &config_path, &port_flag) != 0) return 1;
        port = resolve_port(config_path, port_flag);
        return cli_print_http(port, path);
    }
    if (strcmp(cmd, "board-action") == 0) {
        char path[512];
        int port;
        if (argc < 4) {
            fprintf(stderr, "usage: %s board-action <board> <action> [-c] [-p]\n", argv[0]);
            return 1;
        }
        if (parse_common_flags(argc, argv, 4, &config_path, &port_flag) != 0) return 1;
        port = resolve_port(config_path, port_flag);
        snprintf(path, sizeof(path), "/api/boards/%s/action?action=%s", argv[2], argv[3]);
        return cli_print_http(port, path);
    }
    if (strcmp(cmd, "relay-set") == 0) {
        char path[512];
        int port;
        if (argc < 5) {
            fprintf(stderr, "usage: %s relay-set <relay> <channel> <0|1> [-c] [-p]\n", argv[0]);
            return 1;
        }
        if (parse_common_flags(argc, argv, 5, &config_path, &port_flag) != 0) return 1;
        port = resolve_port(config_path, port_flag);
        snprintf(path, sizeof(path), "/api/relays/%s/relay?channel=%s&state=%s",
                 argv[2], argv[3], argv[4]);
        return cli_print_http(port, path);
    }

    fprintf(stderr, "unknown command: %s\n", cmd);
    print_usage(argv[0]);
    return 1;
}
