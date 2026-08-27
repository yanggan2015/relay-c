#ifndef RELAY_CONFIG_H
#define RELAY_CONFIG_H

#include "relay_types.h"

#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

int  relay_config_load(AppConfig *out, const char *path);
int  relay_config_save(const AppConfig *cfg);
int  relay_config_reload(AppConfig *cfg);
int  relay_config_validate_board(const BoardConfig *board, const AppConfig *cfg, char *err, size_t errcap);
int  relay_config_validate_all(const AppConfig *cfg, char *err, size_t errcap);

int relay_config_relay_create(AppConfig *cfg, const char *name, cJSON *fields, char *err, size_t errcap);
int relay_config_relay_update(AppConfig *cfg, const char *name, const char *new_name, cJSON *fields,
                              char *err, size_t errcap);
int relay_config_relay_delete(AppConfig *cfg, const char *name, char *err, size_t errcap);
int relay_config_board_create(AppConfig *cfg, const char *name, cJSON *fields, char *err, size_t errcap);
int relay_config_board_update(AppConfig *cfg, const char *name, const char *new_name, cJSON *fields,
                              char *err, size_t errcap);
int relay_config_board_delete(AppConfig *cfg, const char *name, char *err, size_t errcap);
int relay_config_server_set_port(AppConfig *cfg, int port, int port_set, char *err, size_t errcap);

#ifdef __cplusplus
}
#endif

#endif
