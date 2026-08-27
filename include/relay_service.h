#ifndef RELAY_SERVICE_H
#define RELAY_SERVICE_H

#include "relay_serial.h"
#include "relay_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RelayService RelayService;

RelayService *relay_service_create(RelayExecutor *ex);
void          relay_service_free(RelayService *svc);

int relay_service_run_relay_direct(RelayService *svc, const RelayConfig *relay,
                                   const char *platform, int channel, int state,
                                   RelayStateCache *cache, int relay_idx,
                                   char *err, size_t errcap);

int relay_service_query_state(RelayService *svc, const RelayConfig *relay,
                              const char *platform, int channel, int *state_out,
                              RelayStateCache *cache, int relay_idx,
                              char *err, size_t errcap);

int relay_service_run_board_action(RelayService *svc, const BoardConfig *board,
                                   const AppConfig *cfg, const char *platform,
                                   const char *action, RelayStateCache *cache,
                                   char *err, size_t errcap);

int relay_service_send_raw(RelayService *svc, const RelayConfig *relay,
                           const char *platform, const uint8_t *data, size_t len,
                           char *err, size_t errcap);

const RelayConfig *relay_find_by_name(const AppConfig *cfg, const char *name);
const BoardConfig *relay_board_find_by_name(const AppConfig *cfg, const char *name);
const BoardCustomAction *relay_board_find_custom_action(const BoardConfig *board, const char *action_name);
int relay_find_index(const AppConfig *cfg, const char *name);

#ifdef __cplusplus
}
#endif

#endif
