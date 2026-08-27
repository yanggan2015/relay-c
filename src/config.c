#include "relay_config.h"
#include "relay_protocol.h"
#include "relay_action.h"
#include "relay_service.h"
#include "util.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int json_bool(cJSON *obj, const char *key, int fallback) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) return fallback;
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v) ? 1 : 0;
    if (cJSON_IsNumber(v)) return v->valueint != 0;
    if (cJSON_IsString(v) && v->valuestring)
        return relay_parse_bool(v->valuestring, fallback);
    return fallback;
}

static int json_int(cJSON *obj, const char *key, int fallback) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(v)) return v->valueint;
    if (cJSON_IsString(v) && v->valuestring)
        return relay_parse_int(v->valuestring, fallback);
    return fallback;
}

static double json_float(cJSON *obj, const char *key, double fallback) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(v)) return v->valuedouble;
    if (cJSON_IsString(v) && v->valuestring)
        return relay_parse_float(v->valuestring, fallback);
    return fallback;
}

static const char *json_str(cJSON *obj, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    return "";
}

static void parse_channel_commands(RelayConfig *relay, cJSON *channels) {
    cJSON *ch;
    if (!cJSON_IsObject(channels)) return;
    for (ch = channels->child; ch; ch = ch->next) {
        int n = atoi(ch->string ? ch->string : "0");
        cJSON *on, *off, *query;
        if (n < 1 || n > RELAY_MAX_CHANNELS || !cJSON_IsObject(ch)) continue;
        on = cJSON_GetObjectItemCaseSensitive(ch, "on");
        off = cJSON_GetObjectItemCaseSensitive(ch, "off");
        query = cJSON_GetObjectItemCaseSensitive(ch, "query");
        if (on && relay_parse_command_json(on, &relay->channels[n].on, n, 1) == 0)
            relay->channels[n].custom = 1;
        if (off && relay_parse_command_json(off, &relay->channels[n].off, n, 0) == 0)
            relay->channels[n].custom = 1;
        if (query && relay_parse_command_json(query, &relay->channels[n].query, n, -1) == 0)
            relay->channels[n].custom = 1;
    }
}

static void parse_relay(const char *name, cJSON *item, RelayConfig *out) {
    memset(out, 0, sizeof(*out));
    relay_str_copy(out->name, sizeof(out->name), name);
    out->enabled = json_bool(item, "enabled", 1);
    relay_str_copy(out->port, sizeof(out->port), json_str(item, "port"));
    relay_str_copy(out->linux_port, sizeof(out->linux_port), json_str(item, "linux_port"));
    relay_str_copy(out->windows_port, sizeof(out->windows_port), json_str(item, "windows_port"));
    out->relay_channels = json_int(item, "relay_channels", 1);
    if (out->relay_channels < 1) out->relay_channels = 1;
    if (out->relay_channels > RELAY_MAX_CHANNELS) out->relay_channels = RELAY_MAX_CHANNELS;
    out->baudrate = json_int(item, "baudrate", 9600);
    out->io_inverted = json_bool(item, "io_inverted", 0);
    {
        const char *si = json_str(item, "startup_init");
        if (!si[0] || relay_str_eq_ci(si, "none") || relay_str_eq_ci(si, "skip"))
            out->startup_init = RELAY_STARTUP_SKIP;
        else if (relay_str_eq_ci(si, "on") || relay_str_eq_ci(si, "true") || relay_str_eq_ci(si, "1"))
            out->startup_init = 1;
        else if (relay_str_eq_ci(si, "off") || relay_str_eq_ci(si, "false") || relay_str_eq_ci(si, "0"))
            out->startup_init = 0;
        else
            out->startup_init = json_bool(item, "startup_init", RELAY_STARTUP_SKIP) ? 1 : RELAY_STARTUP_SKIP;
    }
    out->post_write_delay_ms = json_float(item, "post_write_delay_ms", 100.0);
    parse_channel_commands(out, cJSON_GetObjectItemCaseSensitive(item, "channels"));
}

static void parse_action_binding(cJSON *obj, BoardActionBinding *out, double default_hold) {
    memset(out, 0, sizeof(*out));
    if (!obj) return;
    out->enabled = json_bool(obj, "enabled", 1);
    relay_str_copy(out->relay_name, sizeof(out->relay_name), json_str(obj, "relay"));
    out->channel = json_int(obj, "channel", 1);
    if (out->channel < 1) out->channel = 1;
    out->polarity_inverted = json_bool(obj, "polarity_inverted", 0);
    out->hold_seconds = json_float(obj, "hold_seconds", default_hold);
}

static void parse_custom_action(const char *name, cJSON *obj, BoardCustomAction *out) {
    memset(out, 0, sizeof(*out));
    relay_str_copy(out->name, sizeof(out->name), name);
    relay_str_copy(out->label, sizeof(out->label), json_str(obj, "label"));
    if (!out->label[0]) relay_str_copy(out->label, sizeof(out->label), name);
    out->enabled = json_bool(obj, "enabled", 1);
    relay_str_copy(out->relay_name, sizeof(out->relay_name), json_str(obj, "relay"));
    out->channel = json_int(obj, "channel", 1);
    if (out->channel < 1) out->channel = 1;
    out->polarity_inverted = json_bool(obj, "polarity_inverted", 0);
    out->hold_seconds = json_float(obj, "hold_seconds", 1.0);
    out->mode = relay_action_mode_parse(json_str(obj, "mode"));
}

static void parse_board_actions(BoardConfig *out, cJSON *actions) {
    cJSON *it;
    if (!cJSON_IsObject(actions)) return;
    for (it = actions->child; it && out->n_custom_actions < RELAY_MAX_CUSTOM_ACTIONS; it = it->next) {
        if (!cJSON_IsObject(it) || !it->string) continue;
        parse_custom_action(it->string, it, &out->custom_actions[out->n_custom_actions++]);
    }
}

static void parse_board(const char *name, cJSON *item, BoardConfig *out) {
    cJSON *reset, *upgrade, *maskrom, *actions;
    memset(out, 0, sizeof(*out));
    relay_str_copy(out->name, sizeof(out->name), name);
    out->enabled = json_bool(item, "enabled", 1);

    reset = cJSON_GetObjectItemCaseSensitive(item, "reset");
    parse_action_binding(reset, &out->reset, 5.0);

    upgrade = cJSON_GetObjectItemCaseSensitive(item, "upgrade_mode");
    maskrom = cJSON_GetObjectItemCaseSensitive(item, "maskrom");
    if (upgrade) {
        out->has_upgrade_mode = 1;
        parse_action_binding(upgrade, &out->upgrade_mode, 5.0);
    } else if (maskrom) {
        out->has_upgrade_mode = 1;
        parse_action_binding(maskrom, &out->upgrade_mode, 5.0);
        if (!json_bool(item, "supports_maskrom", 1))
            out->upgrade_mode.enabled = 0;
    } else if (json_bool(item, "supports_maskrom", 0)) {
        out->has_upgrade_mode = 1;
        out->upgrade_mode.enabled = 1;
        relay_str_copy(out->upgrade_mode.relay_name, sizeof(out->upgrade_mode.relay_name),
                       out->reset.relay_name);
        out->upgrade_mode.channel = json_int(item, "maskrom_channel", 1);
    }

    if (!out->reset.relay_name[0]) {
        relay_str_copy(out->reset.relay_name, sizeof(out->reset.relay_name),
                       json_str(item, "relay_name"));
        if (!out->reset.relay_name[0])
            relay_str_copy(out->reset.relay_name, sizeof(out->reset.relay_name),
                           json_str(item, "relay"));
    }
    if (reset && !out->reset.relay_name[0])
        relay_str_copy(out->reset.relay_name, sizeof(out->reset.relay_name),
                       json_str(reset, "relay"));

    actions = cJSON_GetObjectItemCaseSensitive(item, "actions");
    parse_board_actions(out, actions);
}

int relay_config_load(AppConfig *out, const char *path) {
    char *raw;
    cJSON *root, *relays, *boards, *server, *it;
    if (!out || !path) return -1;
    memset(out, 0, sizeof(*out));
    relay_str_copy(out->config_path, sizeof(out->config_path), path);
    relay_str_copy(out->platform, sizeof(out->platform), relay_detect_platform());

    raw = relay_read_file(path, NULL);
    if (!raw) return -1;
    root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;

    server = cJSON_GetObjectItemCaseSensitive(root, "server");
    if (cJSON_IsObject(server) && cJSON_GetObjectItemCaseSensitive(server, "port")) {
        out->server_port = json_int(server, "port", 18053);
        out->server_port_set = 1;
    }

    relays = cJSON_GetObjectItemCaseSensitive(root, "relays");
    if (cJSON_IsObject(relays)) {
        for (it = relays->child; it && out->n_relays < RELAY_MAX_RELAYS; it = it->next) {
            if (!cJSON_IsObject(it) || !it->string) continue;
            parse_relay(it->string, it, &out->relays[out->n_relays++]);
        }
    }

    boards = cJSON_GetObjectItemCaseSensitive(root, "boards");
    if (cJSON_IsObject(boards)) {
        for (it = boards->child; it && out->n_boards < RELAY_MAX_BOARDS; it = it->next) {
            if (!cJSON_IsObject(it) || !it->string) continue;
            parse_board(it->string, it, &out->boards[out->n_boards++]);
        }
    }

    cJSON_Delete(root);
    return 0;
}

static cJSON *command_to_json(const RelayCommand *cmd) {
    cJSON *o = cJSON_CreateObject();
    if (cmd->format == RELAY_FMT_HEX || cmd->format == RELAY_FMT_STR) {
        char hex[RELAY_MAX_CMD_BYTES * 3 + 1];
        size_t i, p = 0;
        for (i = 0; i < cmd->len && p + 3 < sizeof(hex); ++i)
            p += (size_t)snprintf(hex + p, sizeof(hex) - p, "%02X ", cmd->bytes[i]);
        if (p > 0) hex[p - 1] = '\0';
        if (cmd->format == RELAY_FMT_STR)
            cJSON_AddStringToObject(o, "str", (const char *)cmd->bytes);
        else
            cJSON_AddStringToObject(o, "hex", hex);
    }
    return o;
}

static cJSON *channels_to_json(const RelayConfig *r) {
    cJSON *ch = cJSON_CreateObject();
    int i;
    for (i = 1; i <= r->relay_channels; ++i) {
        char key[8];
        cJSON *item;
        if (!r->channels[i].custom) continue;
        item = cJSON_CreateObject();
        if (r->channels[i].on.format != RELAY_FMT_NONE)
            cJSON_AddItemToObject(item, "on", command_to_json(&r->channels[i].on));
        if (r->channels[i].off.format != RELAY_FMT_NONE)
            cJSON_AddItemToObject(item, "off", command_to_json(&r->channels[i].off));
        if (r->channels[i].query.format != RELAY_FMT_NONE)
            cJSON_AddItemToObject(item, "query", command_to_json(&r->channels[i].query));
        snprintf(key, sizeof(key), "%d", i);
        cJSON_AddItemToObject(ch, key, item);
    }
    return ch;
}

static const RelayConfig *cfg_find_relay(const AppConfig *cfg, const char *name) {
    int i;
    if (!cfg || !name) return NULL;
    for (i = 0; i < cfg->n_relays; ++i) {
        if (strcmp(cfg->relays[i].name, name) == 0)
            return &cfg->relays[i];
    }
    return NULL;
}

static cJSON *binding_to_json(const BoardActionBinding *b, int include_hold) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "enabled", b->enabled ? 1 : 0);
    cJSON_AddStringToObject(o, "relay", b->relay_name);
    cJSON_AddNumberToObject(o, "channel", b->channel);
    if (b->polarity_inverted)
        cJSON_AddBoolToObject(o, "polarity_inverted", 1);
    if (include_hold && b->hold_seconds != 5.0)
        cJSON_AddNumberToObject(o, "hold_seconds", b->hold_seconds);
    return o;
}

static cJSON *custom_action_to_json(const BoardCustomAction *a) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "enabled", a->enabled);
    cJSON_AddStringToObject(o, "label", a->label);
    cJSON_AddStringToObject(o, "relay", a->relay_name);
    cJSON_AddNumberToObject(o, "channel", a->channel);
    cJSON_AddStringToObject(o, "mode", relay_action_mode_name(a->mode));
    if (a->polarity_inverted)
        cJSON_AddBoolToObject(o, "polarity_inverted", 1);
    if (a->hold_seconds != 1.0)
        cJSON_AddNumberToObject(o, "hold_seconds", a->hold_seconds);
    return o;
}

int relay_config_save(const AppConfig *cfg) {
    cJSON *root, *relays, *boards, *server;
    char *text;
    int i, j, ok;
    if (!cfg || !cfg->config_path[0]) return -1;

    root = cJSON_CreateObject();
    server = cJSON_CreateObject();
    if (cfg->server_port_set)
        cJSON_AddNumberToObject(server, "port", cfg->server_port);
    cJSON_AddItemToObject(root, "server", server);

    relays = cJSON_CreateObject();
    for (i = 0; i < cfg->n_relays; ++i) {
        const RelayConfig *r = &cfg->relays[i];
        cJSON *item = cJSON_CreateObject();
        if (!r->enabled) cJSON_AddBoolToObject(item, "enabled", 0);
        if (r->port[0]) cJSON_AddStringToObject(item, "port", r->port);
        if (r->linux_port[0]) cJSON_AddStringToObject(item, "linux_port", r->linux_port);
        if (r->windows_port[0]) cJSON_AddStringToObject(item, "windows_port", r->windows_port);
        cJSON_AddNumberToObject(item, "relay_channels", r->relay_channels);
        if (r->baudrate != 9600) cJSON_AddNumberToObject(item, "baudrate", r->baudrate);
        if (r->io_inverted) cJSON_AddBoolToObject(item, "io_inverted", 1);
        if (r->startup_init == 0) cJSON_AddStringToObject(item, "startup_init", "off");
        else if (r->startup_init == 1) cJSON_AddStringToObject(item, "startup_init", "on");
        if (r->post_write_delay_ms != 100.0)
            cJSON_AddNumberToObject(item, "post_write_delay_ms", r->post_write_delay_ms);
        {
            cJSON *ch = channels_to_json(r);
            if (ch->child) cJSON_AddItemToObject(item, "channels", ch);
            else cJSON_Delete(ch);
        }
        cJSON_AddItemToObject(relays, r->name, item);
    }
    cJSON_AddItemToObject(root, "relays", relays);

    boards = cJSON_CreateObject();
    for (i = 0; i < cfg->n_boards; ++i) {
        const BoardConfig *b = &cfg->boards[i];
        cJSON *item = cJSON_CreateObject();
        cJSON *actions;
        if (!b->enabled) cJSON_AddBoolToObject(item, "enabled", 0);
        cJSON_AddItemToObject(item, "reset", binding_to_json(&b->reset, 1));
        if (b->has_upgrade_mode)
            cJSON_AddItemToObject(item, "upgrade_mode", binding_to_json(&b->upgrade_mode, 0));
        if (b->n_custom_actions > 0) {
            actions = cJSON_CreateObject();
            for (j = 0; j < b->n_custom_actions; ++j)
                cJSON_AddItemToObject(actions, b->custom_actions[j].name,
                                      custom_action_to_json(&b->custom_actions[j]));
            cJSON_AddItemToObject(item, "actions", actions);
        }
        cJSON_AddItemToObject(boards, b->name, item);
    }
    cJSON_AddItemToObject(root, "boards", boards);

    text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return -1;
    ok = relay_write_file(cfg->config_path, text, strlen(text));
    free(text);
    return ok == 0 ? 0 : -1;
}

int relay_config_validate_board(const BoardConfig *board, const AppConfig *cfg,
                                char *err, size_t errcap) {
    const RelayConfig *rr, *ur;
    int i;
    if (!board || !cfg) return -1;
    if (!board->reset.enabled) {
        snprintf(err, errcap, "reset action is disabled");
        return -1;
    }
    rr = cfg_find_relay(cfg, board->reset.relay_name);
    if (!rr) {
        snprintf(err, errcap, "reset relay not found: %s", board->reset.relay_name);
        return -1;
    }
    if (!rr->enabled) {
        snprintf(err, errcap, "reset relay is disabled: %s", board->reset.relay_name);
        return -1;
    }
    if (board->reset.channel < 1 || board->reset.channel > rr->relay_channels) {
        snprintf(err, errcap, "reset channel out of range");
        return -1;
    }
    if (board->has_upgrade_mode && board->upgrade_mode.enabled) {
        ur = cfg_find_relay(cfg, board->upgrade_mode.relay_name);
        if (!ur) {
            snprintf(err, errcap, "upgrade_mode relay not found");
            return -1;
        }
        if (!ur->enabled) {
            snprintf(err, errcap, "upgrade_mode relay is disabled");
            return -1;
        }
        if (board->upgrade_mode.channel < 1 || board->upgrade_mode.channel > ur->relay_channels) {
            snprintf(err, errcap, "upgrade_mode channel out of range");
            return -1;
        }
        if (strcmp(rr->name, ur->name) == 0 &&
            board->reset.channel == board->upgrade_mode.channel) {
            snprintf(err, errcap, "upgrade_mode channel must differ from reset on same relay");
            return -1;
        }
    }
    for (i = 0; i < board->n_custom_actions; ++i) {
        const BoardCustomAction *a = &board->custom_actions[i];
        const RelayConfig *cr;
        if (!a->enabled) continue;
        cr = cfg_find_relay(cfg, a->relay_name);
        if (!cr || !cr->enabled) {
            snprintf(err, errcap, "custom action relay not found: %s", a->relay_name);
            return -1;
        }
        if (a->channel < 1 || a->channel > cr->relay_channels) {
            snprintf(err, errcap, "custom action channel out of range: %s", a->name);
            return -1;
        }
    }
    return 0;
}

int relay_config_reload(AppConfig *cfg) {
    AppConfig fresh;
    char path[RELAY_MAX_PATH];
    if (!cfg) return -1;
    relay_str_copy(path, sizeof(path), cfg->config_path);
    memset(&fresh, 0, sizeof(fresh));
    if (relay_config_load(&fresh, path) != 0) return -1;
    *cfg = fresh;
    relay_str_copy(cfg->config_path, sizeof(cfg->config_path), path);
    return 0;
}

int relay_config_validate_all(const AppConfig *cfg, char *err, size_t errcap) {
    int i;
    if (!cfg) return -1;
    for (i = 0; i < cfg->n_boards; ++i) {
        if (relay_config_validate_board(&cfg->boards[i], cfg, err, errcap) != 0)
            return -1;
    }
    return 0;
}

static int find_relay_idx(const AppConfig *cfg, const char *name) {
    int i;
    for (i = 0; i < cfg->n_relays; ++i) {
        if (strcmp(cfg->relays[i].name, name) == 0) return i;
    }
    return -1;
}

static int find_board_idx(const AppConfig *cfg, const char *name) {
    int i;
    for (i = 0; i < cfg->n_boards; ++i) {
        if (strcmp(cfg->boards[i].name, name) == 0) return i;
    }
    return -1;
}

static void rename_relay_refs(AppConfig *cfg, const char *old_name, const char *new_name) {
    int i, j;
    for (i = 0; i < cfg->n_boards; ++i) {
        BoardConfig *b = &cfg->boards[i];
        if (strcmp(b->reset.relay_name, old_name) == 0)
            relay_str_copy(b->reset.relay_name, sizeof(b->reset.relay_name), new_name);
        if (b->has_upgrade_mode && strcmp(b->upgrade_mode.relay_name, old_name) == 0)
            relay_str_copy(b->upgrade_mode.relay_name, sizeof(b->upgrade_mode.relay_name), new_name);
        for (j = 0; j < b->n_custom_actions; ++j) {
            if (strcmp(b->custom_actions[j].relay_name, old_name) == 0)
                relay_str_copy(b->custom_actions[j].relay_name,
                               sizeof(b->custom_actions[j].relay_name), new_name);
        }
    }
}

int relay_config_relay_create(AppConfig *cfg, const char *name, cJSON *fields,
                              char *err, size_t errcap) {
    RelayConfig r;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    if (find_relay_idx(cfg, name) >= 0) {
        snprintf(err, errcap, "relay already exists");
        return -1;
    }
    if (cfg->n_relays >= RELAY_MAX_RELAYS) {
        snprintf(err, errcap, "relay limit reached");
        return -1;
    }
    parse_relay(name, fields ? fields : cJSON_CreateObject(), &r);
    cfg->relays[cfg->n_relays++] = r;
    if (relay_config_save(cfg) != 0) {
        cfg->n_relays--;
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_relay_update(AppConfig *cfg, const char *name, const char *new_name,
                              cJSON *fields, char *err, size_t errcap) {
    int idx;
    RelayConfig r;
    const char *target;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    idx = find_relay_idx(cfg, name);
    if (idx < 0) {
        snprintf(err, errcap, "unknown relay");
        return -1;
    }
    target = (new_name && new_name[0]) ? new_name : name;
    if (strcmp(target, name) != 0 && find_relay_idx(cfg, target) >= 0) {
        snprintf(err, errcap, "target relay exists");
        return -1;
    }
    parse_relay(target, fields ? fields : cJSON_CreateObject(), &r);
    if (strcmp(target, name) != 0) rename_relay_refs(cfg, name, target);
    cfg->relays[idx] = r;
    if (relay_config_save(cfg) != 0) {
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_relay_delete(AppConfig *cfg, const char *name, char *err, size_t errcap) {
    int idx, i, j;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    idx = find_relay_idx(cfg, name);
    if (idx < 0) {
        snprintf(err, errcap, "unknown relay");
        return -1;
    }
    for (i = 0; i < cfg->n_boards; ++i) {
        BoardConfig *b = &cfg->boards[i];
        if (strcmp(b->reset.relay_name, name) == 0 ||
            (b->has_upgrade_mode && strcmp(b->upgrade_mode.relay_name, name) == 0)) {
            snprintf(err, errcap, "relay is used by board: %s", b->name);
            return -1;
        }
        for (j = 0; j < b->n_custom_actions; ++j) {
            if (strcmp(b->custom_actions[j].relay_name, name) == 0) {
                snprintf(err, errcap, "relay is used by action: %s", b->custom_actions[j].name);
                return -1;
            }
        }
    }
    for (i = idx; i < cfg->n_relays - 1; ++i)
        cfg->relays[i] = cfg->relays[i + 1];
    cfg->n_relays--;
    if (relay_config_save(cfg) != 0) {
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_board_create(AppConfig *cfg, const char *name, cJSON *fields,
                              char *err, size_t errcap) {
    BoardConfig b;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    if (find_board_idx(cfg, name) >= 0) {
        snprintf(err, errcap, "board already exists");
        return -1;
    }
    if (cfg->n_boards >= RELAY_MAX_BOARDS) {
        snprintf(err, errcap, "board limit reached");
        return -1;
    }
    parse_board(name, fields ? fields : cJSON_CreateObject(), &b);
    if (relay_config_validate_board(&b, cfg, err, errcap) != 0) return -1;
    cfg->boards[cfg->n_boards++] = b;
    if (relay_config_save(cfg) != 0) {
        cfg->n_boards--;
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_board_update(AppConfig *cfg, const char *name, const char *new_name,
                              cJSON *fields, char *err, size_t errcap) {
    int idx;
    BoardConfig b;
    const char *target;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    idx = find_board_idx(cfg, name);
    if (idx < 0) {
        snprintf(err, errcap, "unknown board");
        return -1;
    }
    target = (new_name && new_name[0]) ? new_name : name;
    if (strcmp(target, name) != 0 && find_board_idx(cfg, target) >= 0) {
        snprintf(err, errcap, "target board exists");
        return -1;
    }
    parse_board(target, fields ? fields : cJSON_CreateObject(), &b);
    if (relay_config_validate_board(&b, cfg, err, errcap) != 0) return -1;
    cfg->boards[idx] = b;
    if (relay_config_save(cfg) != 0) {
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_board_delete(AppConfig *cfg, const char *name, char *err, size_t errcap) {
    int idx, i;
    if (!cfg || !name || !name[0]) {
        snprintf(err, errcap, "name is required");
        return -1;
    }
    idx = find_board_idx(cfg, name);
    if (idx < 0) {
        snprintf(err, errcap, "unknown board");
        return -1;
    }
    for (i = idx; i < cfg->n_boards - 1; ++i)
        cfg->boards[i] = cfg->boards[i + 1];
    cfg->n_boards--;
    if (relay_config_save(cfg) != 0) {
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}

int relay_config_server_set_port(AppConfig *cfg, int port, int port_set, char *err, size_t errcap) {
    if (!cfg) {
        snprintf(err, errcap, "invalid config");
        return -1;
    }
    cfg->server_port = port;
    cfg->server_port_set = port_set;
    if (relay_config_save(cfg) != 0) {
        snprintf(err, errcap, "save failed");
        return -1;
    }
    return 0;
}
