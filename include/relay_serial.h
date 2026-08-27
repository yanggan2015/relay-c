#ifndef RELAY_SERIAL_H
#define RELAY_SERIAL_H

#include "relay_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RelayExecutor RelayExecutor;

RelayExecutor *relay_executor_create(void);
void           relay_executor_free(RelayExecutor *ex);
void           relay_executor_set_dry_run(int enabled);

/* Set channel state using relay config commands */
int relay_executor_set_channel(RelayExecutor *ex, const RelayConfig *relay,
                               const char *port, int channel, int state,
                               RelayStateCache *cache, int relay_idx);

/* Query channel state; returns 0 ok, -1 fail. state_out: 0/1, -1 unknown */
int relay_executor_get_channel(RelayExecutor *ex, const RelayConfig *relay,
                               const char *port, int channel, int *state_out,
                               RelayStateCache *cache, int relay_idx);

/* Send raw bytes */
int relay_executor_send_raw(RelayExecutor *ex, const RelayConfig *relay,
                            const char *port, const uint8_t *data, size_t len);

void relay_executor_sleep(double seconds);

#ifdef __cplusplus
}
#endif

#endif
