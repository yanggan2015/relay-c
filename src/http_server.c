#include "relay_http.h"
#include "relay_config.h"
#include "relay_service.h"
#include "relay_protocol.h"
#include "relay_action.h"
#include "http_api.h"
#include "http_pages.h"
#include "util.h"
#include "mongoose.h"

#include <cjson/cJSON.h>
#include <pthread.h>
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

static void html_reply(struct mg_connection *c, int code, const char *html) {
    mg_http_reply(c, code, "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s", html ? html : "");
}

static void err_json(struct mg_connection *c, int code, const char *msg) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", msg ? msg : "error");
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    json_reply(c, code, s);
    free(s);
}

static void qget(struct mg_http_message *hm, const char *name, char *dst, size_t cap) {
    dst[0] = '\0';
    mg_http_get_var(&hm->query, name, dst, cap);
}

static cJSON *relay_to_json(const RelayConfig *r, const char *platform) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "enabled", r->enabled);
    cJSON_AddStringToObject(o, "port", r->port);
    cJSON_AddStringToObject(o, "linux_port", r->linux_port);
    cJSON_AddStringToObject(o, "windows_port", r->windows_port);
    cJSON_AddNumberToObject(o, "relay_channels", r->relay_channels);
    cJSON_AddNumberToObject(o, "baudrate", r->baudrate);
    cJSON_AddBoolToObject(o, "io_inverted", r->io_inverted);
    cJSON_AddNumberToObject(o, "post_write_delay_ms", r->post_write_delay_ms);
    cJSON_AddStringToObject(o, "resolved_port",
                            relay_resolved_port(r->port, r->linux_port, r->windows_port, platform));
    return o;
}

static cJSON *binding_to_json_api(const BoardActionBinding *b) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "enabled", b->enabled);
    cJSON_AddStringToObject(o, "relay", b->relay_name);
    cJSON_AddNumberToObject(o, "channel", b->channel);
    cJSON_AddBoolToObject(o, "polarity_inverted", b->polarity_inverted);
    cJSON_AddNumberToObject(o, "hold_seconds", b->hold_seconds);
    return o;
}

static cJSON *board_to_json(const BoardConfig *b) {
    cJSON *o = cJSON_CreateObject();
    cJSON *actions;
    int j;
    cJSON_AddBoolToObject(o, "enabled", b->enabled);
    cJSON_AddItemToObject(o, "reset", binding_to_json_api(&b->reset));
    cJSON_AddBoolToObject(o, "has_upgrade_mode", b->has_upgrade_mode);
    if (b->has_upgrade_mode)
        cJSON_AddItemToObject(o, "upgrade_mode", binding_to_json_api(&b->upgrade_mode));
    if (b->n_custom_actions > 0) {
        actions = cJSON_CreateObject();
        for (j = 0; j < b->n_custom_actions; ++j) {
            cJSON *item = cJSON_CreateObject();
            const BoardCustomAction *a = &b->custom_actions[j];
            cJSON_AddBoolToObject(item, "enabled", a->enabled);
            cJSON_AddStringToObject(item, "label", a->label);
            cJSON_AddStringToObject(item, "relay", a->relay_name);
            cJSON_AddNumberToObject(item, "channel", a->channel);
            cJSON_AddStringToObject(item, "mode", relay_action_mode_name(a->mode));
            cJSON_AddItemToObject(actions, a->name, item);
        }
        cJSON_AddItemToObject(o, "actions", actions);
    }
    return o;
}

static void api_relays_list(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *root = cJSON_CreateObject();
    cJSON *relays = cJSON_CreateObject();
    int i;
    for (i = 0; i < srv->cfg->n_relays; ++i)
        cJSON_AddItemToObject(relays, srv->cfg->relays[i].name,
                              relay_to_json(&srv->cfg->relays[i], srv->cfg->platform));
    cJSON_AddItemToObject(root, "relays", relays);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_boards_list(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *root = cJSON_CreateObject();
    cJSON *boards = cJSON_CreateObject();
    int i;
    for (i = 0; i < srv->cfg->n_boards; ++i)
        cJSON_AddItemToObject(boards, srv->cfg->boards[i].name,
                              board_to_json(&srv->cfg->boards[i]));
    cJSON_AddItemToObject(root, "boards", boards);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_boards_support(struct mg_connection *c, RelayHttpServer *srv) {
    cJSON *root = cJSON_CreateObject();
    cJSON *boards = cJSON_CreateObject();
    int i;
    cJSON_AddStringToObject(root, "platform", srv->cfg->platform);
    for (i = 0; i < srv->cfg->n_boards; ++i) {
        const BoardConfig *b = &srv->cfg->boards[i];
        const RelayConfig *rr = relay_find_by_name(srv->cfg, b->reset.relay_name);
        const RelayConfig *ur = b->has_upgrade_mode ?
            relay_find_by_name(srv->cfg, b->upgrade_mode.relay_name) : NULL;
        cJSON *item = cJSON_CreateObject();
        cJSON_AddBoolToObject(item, "enabled", b->enabled);
        cJSON_AddStringToObject(item, "reset_relay_name", b->reset.relay_name);
        if (ur) cJSON_AddStringToObject(item, "upgrade_relay_name", b->upgrade_mode.relay_name);
        cJSON_AddNumberToObject(item, "reset_channel", b->reset.channel);
        if (b->has_upgrade_mode)
            cJSON_AddNumberToObject(item, "upgrade_channel", b->upgrade_mode.channel);
        cJSON_AddBoolToObject(item, "upgrade_supported",
                              b->has_upgrade_mode && b->upgrade_mode.enabled);
        if (rr) {
            cJSON_AddNumberToObject(item, "reset_relay_channels", rr->relay_channels);
            cJSON_AddStringToObject(item, "reset_resolved_port",
                relay_resolved_port(rr->port, rr->linux_port, rr->windows_port, srv->cfg->platform));
        }
        {
            cJSON *actions = cJSON_CreateArray();
            cJSON_AddItemToObject(item, "actions", actions);
            cJSON_AddItemToArray(actions, cJSON_CreateString("reset"));
            if (b->has_upgrade_mode && b->upgrade_mode.enabled)
                cJSON_AddItemToArray(actions, cJSON_CreateString("upgrade"));
            {
                int j;
                for (j = 0; j < b->n_custom_actions; ++j) {
                    if (b->custom_actions[j].enabled)
                        cJSON_AddItemToArray(actions,
                            cJSON_CreateString(b->custom_actions[j].name));
                }
            }
        }
        cJSON_AddItemToObject(boards, b->name, item);
    }
    cJSON_AddItemToObject(root, "boards", boards);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    json_reply(c, 200, s);
    free(s);
}

static void api_relay_control(struct mg_connection *c, RelayHttpServer *srv,
                              const char *relay_name, struct mg_http_message *hm) {
    char qch[16], qst[16];
    const RelayConfig *relay;
    int channel, state, idx;
    char err[256];
    cJSON *resp;

    relay = relay_find_by_name(srv->cfg, relay_name);
    if (!relay) {
        err_json(c, 404, "unknown relay");
        return;
    }
    qget(hm, "channel", qch, sizeof(qch));
    qget(hm, "state", qst, sizeof(qst));
    channel = relay_parse_int(qch, -1);
    state = relay_parse_bool(qst, 0);
    idx = relay_find_index(srv->cfg, relay_name);

    if (relay_service_run_relay_direct(srv->service, relay, srv->cfg->platform,
                                       channel, state, srv->cache, idx, err, sizeof(err)) != 0) {
        err_json(c, 400, err);
        return;
    }
    resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON_AddStringToObject(resp, "action", state ? "relay_on" : "relay_off");
    cJSON_AddNumberToObject(resp, "channel", channel);
    cJSON_AddBoolToObject(resp, "state", state);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    json_reply(c, 200, s);
    free(s);
}

static void api_relay_state(struct mg_connection *c, RelayHttpServer *srv,
                            const char *relay_name, struct mg_http_message *hm) {
    char qch[16];
    const RelayConfig *relay;
    int idx, channel, st;
    char err[256];
    cJSON *resp;

    relay = relay_find_by_name(srv->cfg, relay_name);
    if (!relay) {
        err_json(c, 404, "unknown relay");
        return;
    }
    idx = relay_find_index(srv->cfg, relay_name);
    qget(hm, "channel", qch, sizeof(qch));

    if (qch[0]) {
        channel = relay_parse_int(qch, -1);
        if (relay_service_query_state(srv->service, relay, srv->cfg->platform,
                                      channel, &st, srv->cache, idx, err, sizeof(err)) != 0) {
            err_json(c, 400, err);
            return;
        }
        resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", 1);
        cJSON_AddNumberToObject(resp, "channel", channel);
        if (st < 0) cJSON_AddNullToObject(resp, "state");
        else cJSON_AddBoolToObject(resp, "state", st);
        char *s = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        json_reply(c, 200, s);
        free(s);
        return;
    }

    resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON *states = cJSON_CreateObject();
    for (channel = 1; channel <= relay->relay_channels; ++channel) {
        char key[8];
        snprintf(key, sizeof(key), "%d", channel);
        if (relay_service_query_state(srv->service, relay, srv->cfg->platform,
                                      channel, &st, srv->cache, idx, err, sizeof(err)) == 0 && st >= 0)
            cJSON_AddBoolToObject(states, key, st);
        else
            cJSON_AddNullToObject(states, key);
    }
    cJSON_AddItemToObject(resp, "states", states);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    json_reply(c, 200, s);
    free(s);
}

static void api_relay_raw(struct mg_connection *c, RelayHttpServer *srv,
                          const char *relay_name, struct mg_http_message *hm) {
    char qdata[1024], qfmt[16];
    const RelayConfig *relay;
    uint8_t buf[RELAY_MAX_CMD_BYTES];
    size_t len = 0;
    char err[256];

    relay = relay_find_by_name(srv->cfg, relay_name);
    if (!relay) {
        err_json(c, 404, "unknown relay");
        return;
    }
    qget(hm, "data", qdata, sizeof(qdata));
    qget(hm, "format", qfmt, sizeof(qfmt));
    if (!qdata[0]) {
        err_json(c, 400, "data is required");
        return;
    }
    if (relay_str_eq_ci(qfmt, "str"))
        relay_parse_str_payload(qdata, buf, sizeof(buf), &len);
    else
        relay_parse_hex_payload(qdata, buf, sizeof(buf), &len);

    if (len == 0) {
        err_json(c, 400, "invalid data");
        return;
    }
    if (relay_service_send_raw(srv->service, relay, srv->cfg->platform, buf, len,
                               err, sizeof(err)) != 0) {
        err_json(c, 400, err);
        return;
    }
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddBoolToObject(o, "success", 1);
        cJSON_AddNumberToObject(o, "bytes_sent", (double)len);
        char *s = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        json_reply(c, 200, s);
        free(s);
    }
}

static void api_board_action(struct mg_connection *c, RelayHttpServer *srv,
                             const char *board_name, struct mg_http_message *hm) {
    char qact[RELAY_MAX_NAME];
    const BoardConfig *board;
    char err[256];
    cJSON *resp;

    board = relay_board_find_by_name(srv->cfg, board_name);
    if (!board) {
        err_json(c, 404, "unknown board");
        return;
    }
    qget(hm, "action", qact, sizeof(qact));
    if (relay_service_run_board_action(srv->service, board, srv->cfg, srv->cfg->platform,
                                       qact, srv->cache, err, sizeof(err)) != 0) {
        err_json(c, 400, err);
        return;
    }
    resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON_AddStringToObject(resp, "action", qact);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    json_reply(c, 200, s);
    free(s);
}

static void api_board_state(struct mg_connection *c, RelayHttpServer *srv, const char *board_name) {
    const BoardConfig *board;
    const RelayConfig *rr, *ur;
    int st, ridx, uidx;
    char err[256];
    cJSON *resp, *states;

    board = relay_board_find_by_name(srv->cfg, board_name);
    if (!board) {
        err_json(c, 404, "unknown board");
        return;
    }
    resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON_AddStringToObject(resp, "board", board_name);
    states = cJSON_CreateObject();

    rr = relay_find_by_name(srv->cfg, board->reset.relay_name);
    ridx = relay_find_index(srv->cfg, board->reset.relay_name);
    if (rr && relay_service_query_state(srv->service, rr, srv->cfg->platform,
                                        board->reset.channel, &st, srv->cache, ridx,
                                        err, sizeof(err)) == 0) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "relay", board->reset.relay_name);
        cJSON_AddNumberToObject(item, "channel", board->reset.channel);
        if (st < 0) cJSON_AddStringToObject(item, "state", "unknown");
        else cJSON_AddBoolToObject(item, "state", st);
        cJSON_AddItemToObject(states, "reset", item);
    }

    if (board->has_upgrade_mode && board->upgrade_mode.enabled) {
        ur = relay_find_by_name(srv->cfg, board->upgrade_mode.relay_name);
        uidx = relay_find_index(srv->cfg, board->upgrade_mode.relay_name);
        if (ur && relay_service_query_state(srv->service, ur, srv->cfg->platform,
                                            board->upgrade_mode.channel, &st, srv->cache, uidx,
                                            err, sizeof(err)) == 0) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "relay", board->upgrade_mode.relay_name);
            cJSON_AddNumberToObject(item, "channel", board->upgrade_mode.channel);
            if (st < 0) cJSON_AddStringToObject(item, "state", "unknown");
            else cJSON_AddBoolToObject(item, "state", st);
            cJSON_AddItemToObject(states, "upgrade_mode", item);
        }
    }

    {
        int j;
        for (j = 0; j < board->n_custom_actions; ++j) {
            const BoardCustomAction *a = &board->custom_actions[j];
            const RelayConfig *cr;
            int cidx;
            if (!a->enabled) continue;
            cr = relay_find_by_name(srv->cfg, a->relay_name);
            cidx = relay_find_index(srv->cfg, a->relay_name);
            if (cr && relay_service_query_state(srv->service, cr, srv->cfg->platform,
                                                a->channel, &st, srv->cache, cidx,
                                                err, sizeof(err)) == 0) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "relay", a->relay_name);
                cJSON_AddNumberToObject(item, "channel", a->channel);
                cJSON_AddStringToObject(item, "label", a->label);
                if (st < 0) cJSON_AddStringToObject(item, "state", "unknown");
                else cJSON_AddBoolToObject(item, "state", st);
                cJSON_AddItemToObject(states, a->name, item);
            }
        }
    }

    cJSON_AddItemToObject(resp, "states", states);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    json_reply(c, 200, s);
    free(s);
}

static void dispatch_api(struct mg_connection *c, RelayHttpServer *srv, struct mg_http_message *hm) {
    if (relay_http_api_handle_config(c, srv, hm))
        return;

    struct mg_str uri = hm->uri;
    char relay_name[RELAY_MAX_NAME];
    char board_name[RELAY_MAX_NAME];

    if (uri_eq(uri, "/api/relays")) {
        api_relays_list(c, srv);
        return;
    }
    if (uri_eq(uri, "/api/boards")) {
        api_boards_list(c, srv);
        return;
    }
    if (uri_eq(uri, "/api/boards/support")) {
        api_boards_support(c, srv);
        return;
    }

    if (uri.len > 12 && memcmp(uri.buf, "/api/relays/", 12) == 0) {
        const char *rest = uri.buf + 12;
        size_t rest_len = uri.len - 12;
        size_t i, slash = rest_len;
        for (i = 0; i < rest_len; ++i) {
            if (rest[i] == '/') {
                slash = i;
                break;
            }
        }
        if (slash >= sizeof(relay_name)) slash = sizeof(relay_name) - 1;
        memcpy(relay_name, rest, slash);
        relay_name[slash] = '\0';

        if (slash < rest_len && memcmp(rest + slash + 1, "relay", 5) == 0) {
            api_relay_control(c, srv, relay_name, hm);
            return;
        }
        if (slash < rest_len && memcmp(rest + slash + 1, "state", 5) == 0) {
            api_relay_state(c, srv, relay_name, hm);
            return;
        }
        if (slash < rest_len && memcmp(rest + slash + 1, "raw", 3) == 0) {
            api_relay_raw(c, srv, relay_name, hm);
            return;
        }
    }

    if (uri.len > 12 && memcmp(uri.buf, "/api/boards/", 12) == 0) {
        const char *rest = uri.buf + 12;
        size_t rest_len = uri.len - 12;
        size_t i, slash = rest_len;
        for (i = 0; i < rest_len; ++i) {
            if (rest[i] == '/') {
                slash = i;
                break;
            }
        }
        if (slash >= sizeof(board_name)) slash = sizeof(board_name) - 1;
        memcpy(board_name, rest, slash);
        board_name[slash] = '\0';

        if (slash < rest_len && memcmp(rest + slash + 1, "action", 6) == 0) {
            api_board_action(c, srv, board_name, hm);
            return;
        }
        if (slash < rest_len && memcmp(rest + slash + 1, "state", 5) == 0) {
            api_board_state(c, srv, board_name);
            return;
        }
    }

    err_json(c, 404, "not found");
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    RelayHttpServer *srv = (RelayHttpServer *)c->fn_data;
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (uri_eq(hm->uri, "/")) {
            relay_http_page_control(c, srv);
            return;
        }
        if (uri_eq(hm->uri, "/docs")) {
            relay_http_page_docs(c, srv, hm);
            return;
        }
        if (uri_eq(hm->uri, "/config")) {
            relay_http_page_config(c, srv);
            return;
        }
        if (hm->uri.len >= 5 && memcmp(hm->uri.buf, "/api/", 5) == 0) {
            dispatch_api(c, srv, hm);
            return;
        }
        err_json(c, 404, "not found");
    }
}

RelayHttpServer *relay_http_create(AppConfig *cfg, RelayStateCache *cache) {
    RelayHttpServer *srv = (RelayHttpServer *)calloc(1, sizeof(RelayHttpServer));
    if (!srv) return NULL;
    srv->cfg = cfg;
    srv->cache = cache;
    srv->executor = relay_executor_create();
    srv->service = relay_service_create(srv->executor);
    mg_mgr_init(&srv->mgr);
    return srv;
}

void relay_http_free(RelayHttpServer *srv) {
    if (!srv) return;
    relay_http_stop(srv);
    relay_service_free(srv->service);
    relay_executor_free(srv->executor);
    free(srv);
}

int relay_http_start(RelayHttpServer *srv, int port) {
    char url[64];
    if (!srv) return -1;
    snprintf(url, sizeof(url), "http://0.0.0.0:%d", port);
    if (!mg_http_listen(&srv->mgr, url, fn, srv)) return -1;
    srv->port = port;
    srv->running = 1;
    relay_log("[relay-c] HTTP listening on %s", url);
    return 0;
}

void relay_http_stop(RelayHttpServer *srv) {
    if (!srv || !srv->running) return;
    mg_mgr_free(&srv->mgr);
    mg_mgr_init(&srv->mgr);
    srv->running = 0;
}

int relay_http_poll(RelayHttpServer *srv, int ms) {
    if (!srv || !srv->running) return -1;
    mg_mgr_poll(&srv->mgr, ms);
    return 0;
}

RelayService *relay_http_service(RelayHttpServer *srv) {
    return srv ? srv->service : NULL;
}

AppConfig *relay_http_config(RelayHttpServer *srv) {
    return srv ? srv->cfg : NULL;
}

int relay_http_port(RelayHttpServer *srv) {
    return srv ? srv->port : 0;
}
