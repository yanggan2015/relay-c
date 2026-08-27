#include "relay_protocol.h"
#include "util.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

uint8_t relay_a0_checksum(const uint8_t *data, size_t len) {
    unsigned sum = 0;
    size_t i;
    for (i = 0; i < len; ++i) sum += data[i];
    return (uint8_t)(sum & 0xFF);
}

int relay_build_a0_set(RelayCommand *cmd, int channel, int on) {
    uint8_t data[3];
    if (!cmd || channel < 1) return -1;
    data[0] = 0xA0;
    data[1] = (uint8_t)channel;
    data[2] = on ? 0x01 : 0x00;
    cmd->format = RELAY_FMT_A0;
    cmd->a0_on = on ? 1 : 0;
    cmd->len = 4;
    cmd->bytes[0] = data[0];
    cmd->bytes[1] = data[1];
    cmd->bytes[2] = data[2];
    cmd->bytes[3] = relay_a0_checksum(data, 3);
    cmd->has_response = 0;
    return 0;
}

int relay_build_a0_query(RelayCommand *cmd, int channel) {
    uint8_t data[3];
    if (!cmd || channel < 1) return -1;
    data[0] = 0xA0;
    data[1] = (uint8_t)channel;
    data[2] = 0x05;
    cmd->format = RELAY_FMT_A0;
    cmd->len = 4;
    cmd->bytes[0] = data[0];
    cmd->bytes[1] = data[1];
    cmd->bytes[2] = data[2];
    cmd->bytes[3] = relay_a0_checksum(data, 3);
    cmd->has_response = 1;
    cmd->resp_offset = 2;
    cmd->resp_on_val = 0x01;
    return 0;
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int relay_parse_hex_payload(const char *hex, uint8_t *out, size_t cap, size_t *out_len) {
    size_t n = 0;
    int hi = -1;
    const char *p;
    if (!hex || !out || !out_len) return -1;
    for (p = hex; *p; ++p) {
        if (isspace((unsigned char)*p)) continue;
        if (*p == ':' || *p == '-') continue;
        if (n >= cap) return -1;
        if (hi < 0) {
            hi = hex_nibble(*p);
            if (hi < 0) return -1;
        } else {
            int lo = hex_nibble(*p);
            if (lo < 0) return -1;
            out[n++] = (uint8_t)((hi << 4) | lo);
            hi = -1;
        }
    }
    if (hi >= 0) return -1;
    *out_len = n;
    return 0;
}

int relay_parse_str_payload(const char *str, uint8_t *out, size_t cap, size_t *out_len) {
    size_t n = 0;
    const char *p;
    if (!str || !out || !out_len) return -1;
    for (p = str; *p; ++p) {
        if (n >= cap) return -1;
        if (*p == '\\' && p[1]) {
            ++p;
            if (*p == 'r') out[n++] = '\r';
            else if (*p == 'n') out[n++] = '\n';
            else if (*p == 't') out[n++] = '\t';
            else if (*p == '\\') out[n++] = '\\';
            else if (*p == 'x' && p[1] && p[2]) {
                int hi = hex_nibble(p[1]);
                int lo = hex_nibble(p[2]);
                if (hi < 0 || lo < 0) return -1;
                out[n++] = (uint8_t)((hi << 4) | lo);
                p += 2;
            } else out[n++] = (uint8_t)*p;
        } else {
            out[n++] = (uint8_t)*p;
        }
    }
    *out_len = n;
    return 0;
}

int relay_parse_command_json(void *json_obj, RelayCommand *cmd, int channel, int is_on) {
    cJSON *obj = (cJSON *)json_obj;
    cJSON *v;
    if (!obj || !cmd) return -1;
    memset(cmd, 0, sizeof(*cmd));

    v = cJSON_GetObjectItemCaseSensitive(obj, "hex");
    if (cJSON_IsString(v) && v->valuestring) {
        cmd->format = RELAY_FMT_HEX;
        return relay_parse_hex_payload(v->valuestring, cmd->bytes, sizeof(cmd->bytes), &cmd->len);
    }

    v = cJSON_GetObjectItemCaseSensitive(obj, "str");
    if (cJSON_IsString(v) && v->valuestring) {
        cmd->format = RELAY_FMT_STR;
        return relay_parse_str_payload(v->valuestring, cmd->bytes, sizeof(cmd->bytes), &cmd->len);
    }

    v = cJSON_GetObjectItemCaseSensitive(obj, "a0");
    if (cJSON_IsBool(v) || cJSON_IsTrue(v) || cJSON_IsFalse(v)) {
        return relay_build_a0_set(cmd, channel, cJSON_IsTrue(v) ? 1 : 0);
    }
    if (cJSON_IsObject(obj) && cJSON_GetObjectItemCaseSensitive(obj, "query")) {
        return relay_build_a0_query(cmd, channel);
    }

    /* bare string value treated as hex if object has only one string field named data */
    v = cJSON_GetObjectItemCaseSensitive(obj, "data");
    if (cJSON_IsString(v) && v->valuestring) {
        if (relay_str_eq_ci(cJSON_GetObjectItemCaseSensitive(obj, "type") ?
                            cJSON_GetObjectItemCaseSensitive(obj, "type")->valuestring : "", "str")) {
            cmd->format = RELAY_FMT_STR;
            return relay_parse_str_payload(v->valuestring, cmd->bytes, sizeof(cmd->bytes), &cmd->len);
        }
        cmd->format = RELAY_FMT_HEX;
        return relay_parse_hex_payload(v->valuestring, cmd->bytes, sizeof(cmd->bytes), &cmd->len);
    }

    /* default A0 for on/off */
    if (is_on >= 0) return relay_build_a0_set(cmd, channel, is_on);
    return relay_build_a0_query(cmd, channel);
}

int relay_resolve_command(const RelayConfig *relay, int channel, int is_on,
                          int is_query, RelayCommand *out) {
    const ChannelCommands *ch;
    const RelayCommand *src;
    if (!relay || !out || channel < 1 || channel > relay->relay_channels) return -1;
    ch = &relay->channels[channel];
    if (ch->custom) {
        if (is_query && ch->query.format != RELAY_FMT_NONE) {
            src = &ch->query;
        } else if (is_on && ch->on.format != RELAY_FMT_NONE) {
            src = &ch->on;
        } else if (!is_on && ch->off.format != RELAY_FMT_NONE) {
            src = &ch->off;
        } else {
            src = NULL;
        }
        if (src) {
            *out = *src;
            return 0;
        }
    }
    if (is_query) return relay_build_a0_query(out, channel);
    return relay_build_a0_set(out, channel, is_on ? 1 : 0);
}

int relay_parse_a0_response(const uint8_t *data, size_t len, int channel, int *state_out) {
    if (!data || !state_out || len < 4) return -1;
    if (data[0] != 0xA0 || data[1] != (uint8_t)channel) return -1;
    if (data[2] != 0x00 && data[2] != 0x01) return -1;
    *state_out = (data[2] == 0x01) ? 1 : 0;
    return 0;
}
