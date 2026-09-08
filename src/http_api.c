#include "http_api.h"
#include "relay_ports.h"
#include "relay_config.h"
#include "relay_service.h"
#include "relay_serial.h"
#include "relay_action.h"
#include "util.h"
#include "mongoose.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RelayHttpServer {
    AppConfig       *cfg;
    RelayStateCache *cache;
    RelayExecutor   *executor;
    RelayService    *service;
    struct mg_mgr    mgr;
    int              port;
    volatile int     running;
};

static int uri_eq(struct mg_str u, const char *p) {
    size_t n = strlen(p);
    return u.len == n && memcmp(u.buf, p, n) == 0;
}

static void json_reply(struct mg_connection *c, int code, const char *json) {
    mg_http_reply(c, code,
                  "Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s", json ? json : "{}");
}

static void err_json(struct mg_connection *c, int code, const char *msg) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", msg ? msg : "error");
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_reply(c, code, s);
    free(s);
}

static void ok_json(struct mg_connection *c, int code) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "success", 1);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_reply(c, code, s);
    free(s);
}

static void qget(struct mg_http_message *hm, const char *name, char *dst, size_t cap) {
    dst[0] = '\0';
    mg_http_get_var(&hm->query, name, dst, cap);
}

static const char *startup_init_name(int v) {
    if (v == 0) return "off";
    if (v == 1) return "on";
    return "none";
}

static cJSON *query_to_relay_json(struct mg_http_message *hm) {
    char buf[256];
    cJSON *o = cJSON_CreateObject();
    qget(hm, "port", buf, sizeof(buf));
    if (buf[0]) cJSON_AddStringToObject(o, "port", buf);
    qget(hm, "windows_port", buf, sizeof(buf));
    if (buf[0]) cJSON_AddStringToObject(o, "windows_port", buf);
    qget(hm, "linux_port", buf, sizeof(buf));
    if (buf[0]) cJSON_AddStringToObject(o, "linux_port", buf);
    qget(hm, "relay_channels", buf, sizeof(buf));
    if (buf[0]) cJSON_AddNumberToObject(o, "relay_channels", atoi(buf));
    qget(hm, "baudrate", buf, sizeof(buf));
    if (buf[0]) cJSON_AddNumberToObject(o, "baudrate", atoi(buf));
    qget(hm, "enabled", buf, sizeof(buf));
    if (buf[0]) cJSON_AddBoolToObject(o, "enabled", relay_parse_bool(buf, 1));
    qget(hm, "io_inverted", buf, sizeof(buf));
    if (buf[0]) cJSON_AddBoolToObject(o, "io_inverted", relay_parse_bool(buf, 0));
    qget(hm, "startup_init", buf, sizeof(buf));
    if (buf[0]) cJSON_AddStringToObject(o, "startup_init", buf);
    return o;
}

static cJSON *query_to_board_json(struct mg_http_message *hm) {
    char buf[256];
    char actions_json[8192];
    char upg_relay[256];
    cJSON *o = cJSON_CreateObject();
    cJSON *reset = cJSON_CreateObject();
    cJSON *upgrade = NULL;
    int upg_explicit = 0;
    int upg_enabled = 0;
    int upg_ch = 2;
    int upg_pol = 0;

    qget(hm, "enabled", buf, sizeof(buf));
    if (buf[0]) cJSON_AddBoolToObject(o, "enabled", relay_parse_bool(buf, 1));
    qget(hm, "reset_relay_name", buf, sizeof(buf));
    if (!buf[0]) qget(hm, "relay", buf, sizeof(buf));
    if (buf[0]) cJSON_AddStringToObject(reset, "relay", buf);
    qget(hm, "reset_channel", buf, sizeof(buf));
    if (buf[0]) cJSON_AddNumberToObject(reset, "channel", atoi(buf));
    qget(hm, "reset_hold_seconds", buf, sizeof(buf));
    if (buf[0]) cJSON_AddNumberToObject(reset, "hold_seconds", atof(buf));
    qget(hm, "reset_polarity_inverted", buf, sizeof(buf));
    if (buf[0]) cJSON_AddBoolToObject(reset, "polarity_inverted", relay_parse_bool(buf, 0));
    cJSON_AddItemToObject(o, "reset", reset);

    upg_relay[0] = '\0';
    qget(hm, "upgrade_enabled", buf, sizeof(buf));
    if (buf[0]) {
        upg_explicit = 1;
        upg_enabled = relay_parse_bool(buf, 0);
    }
    qget(hm, "upgrade_relay_name", buf, sizeof(buf));
    if (!buf[0]) qget(hm, "maskrom_relay_name", buf, sizeof(buf));
    if (buf[0]) relay_str_copy(upg_relay, sizeof(upg_relay), buf);
    qget(hm, "upgrade_channel", buf, sizeof(buf));
    if (!buf[0]) qget(hm, "maskrom_channel", buf, sizeof(buf));
    if (buf[0]) upg_ch = atoi(buf);
    qget(hm, "upgrade_polarity_inverted", buf, sizeof(buf));
    if (buf[0]) upg_pol = relay_parse_bool(buf, 0);

    if (upg_explicit || upg_relay[0]) {
        upgrade = cJSON_CreateObject();
        cJSON_AddBoolToObject(upgrade, "enabled", upg_enabled);
        if (upg_relay[0]) cJSON_AddStringToObject(upgrade, "relay", upg_relay);
        cJSON_AddNumberToObject(upgrade, "channel", upg_ch);
        cJSON_AddBoolToObject(upgrade, "polarity_inverted", upg_pol);
        cJSON_AddItemToObject(o, "upgrade_mode", upgrade);
    }

    qget(hm, "actions_json", actions_json, sizeof(actions_json));
    if (actions_json[0]) {
        cJSON *actions = cJSON_Parse(actions_json);
        if (actions && cJSON_IsObject(actions))
            cJSON_AddItemToObject(o, "actions", actions);
        else if (actions)
            cJSON_Delete(actions);
    }
    return o;
}

static void api_health(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddStringToObject(o, "version", RELAY_VERSION);
    cJSON_AddStringToObject(o, "platform", srv->cfg->platform);
    cJSON_AddNumberToObject(o, "relays", srv->cfg->n_relays);
    cJSON_AddNumberToObject(o, "boards", srv->cfg->n_boards);
    cJSON_AddNumberToObject(o, "http_port", srv->port);
    cJSON_AddStringToObject(o, "config_path", srv->cfg->config_path);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_reply(c, 200, s);
    free(s);
}

static void api_config_reload(struct mg_connection *c, RelayHttpServer *srv) {
    char err[256];
    if (relay_http_reload_config(srv, err, sizeof(err)) != 0) {
        err_json(c, 500, err);
        return;
    }
    ok_json(c, 200);
}

static void api_config_relays(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *root = cJSON_CreateObject();
    cJSON *relays = cJSON_CreateObject();
    int i;
    for (i = 0; i < srv->cfg->n_relays; ++i) {
        RelayConfig *r = &srv->cfg->relays[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddBoolToObject(item, "enabled", r->enabled);
        cJSON_AddStringToObject(item, "windows_port", r->windows_port);
        cJSON_AddStringToObject(item, "linux_port", r->linux_port);
        cJSON_AddNumberToObject(item, "relay_channels", r->relay_channels);
        cJSON_AddNumberToObject(item, "baudrate", r->baudrate);
        cJSON_AddBoolToObject(item, "io_inverted", r->io_inverted);
        cJSON_AddStringToObject(item, "startup_init", startup_init_name(r->startup_init));
        cJSON_AddItemToObject(relays, r->name, item);
    }
    cJSON_AddItemToObject(root, "relays", relays);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_config_boards(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *root = cJSON_CreateObject();
    cJSON *boards = cJSON_CreateObject();
    int i;
    for (i = 0; i < srv->cfg->n_boards; ++i) {
        BoardConfig *b = &srv->cfg->boards[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddBoolToObject(item, "enabled", b->enabled);
        cJSON_AddStringToObject(item, "reset_relay", b->reset.relay_name);
        cJSON_AddNumberToObject(item, "reset_channel", b->reset.channel);
        cJSON_AddItemToObject(boards, b->name, item);
    }
    cJSON_AddItemToObject(root, "boards", boards);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_serial_ports(struct mg_connection *c) {
    cJSON *root = relay_ports_enumerate_json();
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_help(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *o = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    char base[128];
    char *s;
    snprintf(base, sizeof(base), "http://127.0.0.1:%d", srv->port);
    cJSON_AddStringToObject(o, "tool", "relay");
    cJSON_AddStringToObject(o, "version", RELAY_VERSION);
    cJSON_AddStringToObject(o, "base", base);
#define ADD_CMD(name, path, demo) do { \
    cJSON *it = cJSON_CreateObject(); \
    cJSON_AddStringToObject(it, "name", name); \
    cJSON_AddStringToObject(it, "path", path); \
    cJSON_AddStringToObject(it, "example", demo); \
    cJSON_AddItemToArray(arr, it); \
} while (0)
    ADD_CMD("health", "/api/health or /health",
            "curl \"http://127.0.0.1:PORT/api/health\"");
    ADD_CMD("help", "/api/help",
            "curl \"http://127.0.0.1:PORT/api/help\"");
    ADD_CMD("list relays", "/api/relays",
            "curl \"http://127.0.0.1:PORT/api/relays\"");
    ADD_CMD("list boards", "/api/boards",
            "curl \"http://127.0.0.1:PORT/api/boards\"");
    ADD_CMD("boards support", "/api/boards/support",
            "curl \"http://127.0.0.1:PORT/api/boards/support\"");
    ADD_CMD("relay on/off", "/api/relays/{name}/relay?channel=&state=",
            "curl \"http://127.0.0.1:PORT/api/relays/NAME/relay?channel=1&state=1\"");
    ADD_CMD("relay state", "/api/relays/{name}/state?channel=",
            "curl \"http://127.0.0.1:PORT/api/relays/NAME/state\"");
    ADD_CMD("relay raw", "/api/relays/{name}/raw?data=&format=",
            "curl \"http://127.0.0.1:PORT/api/relays/NAME/raw?data=A0%2001%2001%20A2\"");
    ADD_CMD("board action", "/api/boards/{name}/action?action=",
            "curl \"http://127.0.0.1:PORT/api/boards/NAME/action?action=reset\"");
    ADD_CMD("board state", "/api/boards/{name}/state",
            "curl \"http://127.0.0.1:PORT/api/boards/NAME/state\"");
    ADD_CMD("serial ports", "/api/serial/ports",
            "curl \"http://127.0.0.1:PORT/api/serial/ports\"");
    ADD_CMD("config reload", "/api/config/reload",
            "curl \"http://127.0.0.1:PORT/api/config/reload\"");
    ADD_CMD("config relays", "/api/config/relays",
            "curl \"http://127.0.0.1:PORT/api/config/relays\"");
    ADD_CMD("config boards", "/api/config/boards",
            "curl \"http://127.0.0.1:PORT/api/config/boards\"");
#undef ADD_CMD
    cJSON_AddItemToObject(o, "commands", arr);
    s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_reply(c, 200, s);
    free(s);
}

static int handle_config_routes(struct mg_connection *c, RelayHttpServer *srv,
                                struct mg_http_message *hm) {
    char name[RELAY_MAX_NAME];
    char new_name[RELAY_MAX_NAME];
    char err[256];
    cJSON *fields;

    if (uri_eq(hm->uri, "/api/health") || uri_eq(hm->uri, "/health")) {
        api_health(c, srv);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/help")) {
        api_help(c, srv);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/serial/ports")) {
        api_serial_ports(c);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/reload")) {
        api_config_reload(c, srv);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/relays")) {
        api_config_relays(c, srv);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/boards")) {
        api_config_boards(c, srv);
        return 1;
    }

    if (uri_eq(hm->uri, "/api/config/relay/create")) {
        qget(hm, "name", name, sizeof(name));
        fields = query_to_relay_json(hm);
        if (relay_config_relay_create(srv->cfg, name, fields, err, sizeof(err)) != 0) {
            cJSON_Delete(fields);
            err_json(c, 400, err);
            return 1;
        }
        cJSON_Delete(fields);
        ok_json(c, 201);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/relay/update")) {
        qget(hm, "name", name, sizeof(name));
        qget(hm, "new_name", new_name, sizeof(new_name));
        fields = query_to_relay_json(hm);
        if (relay_config_relay_update(srv->cfg, name, new_name, fields, err, sizeof(err)) != 0) {
            cJSON_Delete(fields);
            err_json(c, 400, err);
            return 1;
        }
        cJSON_Delete(fields);
        ok_json(c, 200);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/relay/delete")) {
        qget(hm, "name", name, sizeof(name));
        if (relay_config_relay_delete(srv->cfg, name, err, sizeof(err)) != 0) {
            err_json(c, 400, err);
            return 1;
        }
        ok_json(c, 200);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/board/create")) {
        qget(hm, "name", name, sizeof(name));
        fields = query_to_board_json(hm);
        if (relay_config_board_create(srv->cfg, name, fields, err, sizeof(err)) != 0) {
            cJSON_Delete(fields);
            err_json(c, 400, err);
            return 1;
        }
        cJSON_Delete(fields);
        ok_json(c, 201);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/board/update")) {
        qget(hm, "name", name, sizeof(name));
        qget(hm, "new_name", new_name, sizeof(new_name));
        fields = query_to_board_json(hm);
        if (relay_config_board_update(srv->cfg, name, new_name, fields, err, sizeof(err)) != 0) {
            cJSON_Delete(fields);
            err_json(c, 400, err);
            return 1;
        }
        cJSON_Delete(fields);
        ok_json(c, 200);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/board/delete")) {
        qget(hm, "name", name, sizeof(name));
        if (relay_config_board_delete(srv->cfg, name, err, sizeof(err)) != 0) {
            err_json(c, 400, err);
            return 1;
        }
        ok_json(c, 200);
        return 1;
    }
    if (uri_eq(hm->uri, "/api/config/server/set")) {
        char pbuf[32];
        int port, port_set = 0;
        qget(hm, "port", pbuf, sizeof(pbuf));
        if (!pbuf[0]) {
            port = 0;
            port_set = 0;
        } else {
            port = atoi(pbuf);
            if (port <= 0 || port > 65535) {
                err_json(c, 400, "port out of range");
                return 1;
            }
            port_set = 1;
        }
        if (relay_config_server_set_port(srv->cfg, port, port_set, err, sizeof(err)) != 0) {
            err_json(c, 400, err);
            return 1;
        }
        ok_json(c, 200);
        return 1;
    }
    return 0;
}

int relay_http_api_handle_config(struct mg_connection *c, RelayHttpServer *srv,
                                 struct mg_http_message *hm) {
    return handle_config_routes(c, srv, hm);
}

int relay_http_reload_config(RelayHttpServer *srv, char *err, size_t errcap) {
    if (!srv || !srv->cfg) {
        snprintf(err, errcap, "invalid server");
        return -1;
    }
    if (relay_config_reload(srv->cfg) != 0) {
        snprintf(err, errcap, "reload failed");
        return -1;
    }
    if (relay_config_validate_all(srv->cfg, err, errcap) != 0)
        relay_log("[config] reload warning: %s", err);
    relay_log("[config] reloaded from %s", srv->cfg->config_path);
    return 0;
}
