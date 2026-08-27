#ifndef RELAY_PROTOCOL_H
#define RELAY_PROTOCOL_H

#include "relay_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t relay_a0_checksum(const uint8_t *data, size_t len);

/* Build A0 set/query frames into cmd->bytes */
int relay_build_a0_set(RelayCommand *cmd, int channel, int on);
int relay_build_a0_query(RelayCommand *cmd, int channel);

/* Parse "A0 01 01 A2" or "A00101A2" hex string into bytes */
int relay_parse_hex_payload(const char *hex, uint8_t *out, size_t cap, size_t *out_len);

/* Parse str with \r \n \xHH escapes */
int relay_parse_str_payload(const char *str, uint8_t *out, size_t cap, size_t *out_len);

/* Parse command object: { "hex": "..." } or { "str": "..." } or { "a0": true/false } */
int relay_parse_command_json(void *json_obj, RelayCommand *cmd, int channel, int is_on);

/* Resolve command for channel+action; builds A0 if no custom */
int relay_resolve_command(const RelayConfig *relay, int channel, int is_on,
                          int is_query, RelayCommand *out);

/* Parse A0 query response */
int relay_parse_a0_response(const uint8_t *data, size_t len, int channel, int *state_out);

#ifdef __cplusplus
}
#endif

#endif
