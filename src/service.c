#include "relay_service.h"
#include "relay_action.h"
#include "relay_lock.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RelayService {
    RelayExecutor *executor;
};

RelayService *relay_service_create(RelayExecutor *ex) {
    RelayService *svc = (RelayService *)calloc(1, sizeof(RelayService));
    if (svc) svc->executor = ex;
    return svc;
}

void relay_service_free(RelayService *svc) {
    free(svc);
}

const RelayConfig *relay_find_by_name(const AppConfig *cfg, const char *name) {
    int i;
    if (!cfg || !name) return NULL;
    for (i = 0; i < cfg->n_relays; ++i) {
        if (strcmp(cfg->relays[i].name, name) == 0)
            return &cfg->relays[i];
    }
    return NULL;
}

const BoardConfig *relay_board_find_by_name(const AppConfig *cfg, const char *name) {
    int i;
    if (!cfg || !name) return NULL;
    for (i = 0; i < cfg->n_boards; ++i) {
        if (strcmp(cfg->boards[i].name, name) == 0)
            return &cfg->boards[i];
    }
    return NULL;
}

const BoardCustomAction *relay_board_find_custom_action(const BoardConfig *board,
                                                        const char *action_name) {
    int i;
    if (!board || !action_name) return NULL;
    for (i = 0; i < board->n_custom_actions; ++i) {
        if (strcmp(board->custom_actions[i].name, action_name) == 0)
            return &board->custom_actions[i];
    }
    return NULL;
}

int relay_find_index(const AppConfig *cfg, const char *name) {
    int i;
    if (!cfg || !name) return -1;
    for (i = 0; i < cfg->n_relays; ++i) {
        if (strcmp(cfg->relays[i].name, name) == 0) return i;
    }
    return -1;
}

static const char *resolve_port(const RelayConfig *relay, const char *platform,
                                char *err, size_t errcap) {
    const char *p = relay_resolved_port(relay->port, relay->linux_port,
                                        relay->windows_port, platform);
    if (!p || !p[0]) {
        snprintf(err, errcap, "no port configured");
        return NULL;
    }
    return p;
}

static int set_logical(RelayService *svc, const RelayConfig *relay, const char *port,
                       int channel, int logical, RelayStateCache *cache, int relay_idx) {
    return relay_executor_set_channel(svc->executor, relay, port, channel, logical,
                                      cache, relay_idx);
}

/*
 * 固定 reset 脉冲时序（逻辑电平）：
 *   polarity_inverted=false: 空闲=ON -> 脉冲=OFF -> 保持 -> 恢复 ON
 *   polarity_inverted=true:  空闲=OFF -> 脉冲=ON  -> 保持 -> 恢复 OFF
 */
static int run_pulse_sequence(RelayService *svc, const RelayConfig *relay,
                              const char *port, int channel, double hold_seconds,
                              int polarity_inverted, RelayStateCache *cache, int relay_idx) {
    int idle, pulse;
    relay_action_idle_pulse(polarity_inverted, &idle, &pulse);
    if (set_logical(svc, relay, port, channel, pulse, cache, relay_idx) != 0)
        return -1;
    relay_executor_sleep(hold_seconds);
    if (set_logical(svc, relay, port, channel, idle, cache, relay_idx) != 0)
        return -1;
    return 0;
}

static int run_custom_action(RelayService *svc, const BoardCustomAction *act,
                             const AppConfig *cfg, const char *platform,
                             RelayStateCache *cache, char *err, size_t errcap) {
    const RelayConfig *relay;
    const char *port;
    int relay_idx, idle, pulse, target;

    if (!act || !act->enabled) {
        snprintf(err, errcap, "custom action disabled");
        return -1;
    }
    relay = relay_find_by_name(cfg, act->relay_name);
    if (!relay || !relay->enabled) {
        snprintf(err, errcap, "custom action relay not available");
        return -1;
    }
    if (act->channel < 1 || act->channel > relay->relay_channels) {
        snprintf(err, errcap, "custom action channel out of range");
        return -1;
    }
    port = resolve_port(relay, platform, err, errcap);
    if (!port) return -1;
    relay_idx = relay_find_index(cfg, relay->name);
    relay_action_idle_pulse(act->polarity_inverted, &idle, &pulse);

    switch (act->mode) {
    case ACTION_MODE_PULSE:
        return run_pulse_sequence(svc, relay, port, act->channel, act->hold_seconds,
                                  act->polarity_inverted, cache, relay_idx);
    case ACTION_MODE_HOLD_ON:
        target = 1;
        return set_logical(svc, relay, port, act->channel, target, cache, relay_idx);
    case ACTION_MODE_HOLD_OFF:
        target = 0;
        return set_logical(svc, relay, port, act->channel, target, cache, relay_idx);
    default:
        snprintf(err, errcap, "unknown action mode");
        return -1;
    }
}

int relay_service_run_relay_direct(RelayService *svc, const RelayConfig *relay,
                                   const char *platform, int channel, int state,
                                   RelayStateCache *cache, int relay_idx,
                                   char *err, size_t errcap) {
    const char *port;
    if (!svc || !relay) {
        snprintf(err, errcap, "invalid args");
        return -1;
    }
    if (!relay->enabled) {
        snprintf(err, errcap, "relay is disabled");
        return -1;
    }
    if (channel < 1 || channel > relay->relay_channels) {
        snprintf(err, errcap, "channel out of range");
        return -1;
    }
    port = resolve_port(relay, platform, err, errcap);
    if (!port) return -1;
    if (relay_executor_set_channel(svc->executor, relay, port, channel, state ? 1 : 0,
                                   cache, relay_idx) != 0) {
        snprintf(err, errcap, "serial write failed");
        return -1;
    }
    return 0;
}

int relay_service_query_state(RelayService *svc, const RelayConfig *relay,
                              const char *platform, int channel, int *state_out,
                              RelayStateCache *cache, int relay_idx,
                              char *err, size_t errcap) {
    const char *port;
    if (!svc || !relay || !state_out) {
        snprintf(err, errcap, "invalid args");
        return -1;
    }
    if (!relay->enabled) {
        snprintf(err, errcap, "relay is disabled");
        return -1;
    }
    if (channel < 1 || channel > relay->relay_channels) {
        snprintf(err, errcap, "channel out of range");
        return -1;
    }
    port = resolve_port(relay, platform, err, errcap);
    if (!port) return -1;
    if (relay_executor_get_channel(svc->executor, relay, port, channel, state_out,
                                    cache, relay_idx) != 0) {
        snprintf(err, errcap, "state query failed");
        return -1;
    }
    return 0;
}

int relay_service_send_raw(RelayService *svc, const RelayConfig *relay,
                           const char *platform, const uint8_t *data, size_t len,
                           char *err, size_t errcap) {
    const char *port;
    if (!svc || !relay || !data || len == 0) {
        snprintf(err, errcap, "invalid args");
        return -1;
    }
    if (!relay->enabled) {
        snprintf(err, errcap, "relay is disabled");
        return -1;
    }
    port = resolve_port(relay, platform, err, errcap);
    if (!port) return -1;
    if (relay_executor_send_raw(svc->executor, relay, port, data, len) != 0) {
        snprintf(err, errcap, "raw send failed");
        return -1;
    }
    return 0;
}

/*
 * 固定 upgrade_mode 时序（与 Python maskrom 一致，使用 pulse/idle 语义）：
 * 1. reset 进入脉冲态（assert）
 * 2. upgrade 进入脉冲态（enable）
 * 3. 等待 2s
 * 4. reset 恢复空闲态（de-assert）
 * 5. 等待 2s
 * 6. upgrade 恢复空闲态（disable）
 */
static int run_upgrade_sequence(RelayService *svc, const BoardConfig *board,
                                const AppConfig *cfg, const char *platform,
                                RelayStateCache *cache, char *err, size_t errcap) {
    const RelayConfig *reset_relay, *upgrade_relay;
    const char *reset_port, *upgrade_port;
    int reset_idx, upgrade_idx;
    int reset_idle, reset_pulse, upgrade_idle, upgrade_pulse;

    if (!board->has_upgrade_mode || !board->upgrade_mode.enabled) {
        snprintf(err, errcap, "upgrade_mode not supported or disabled");
        return -1;
    }

    reset_relay = relay_find_by_name(cfg, board->reset.relay_name);
    upgrade_relay = relay_find_by_name(cfg, board->upgrade_mode.relay_name);
    if (!reset_relay || !reset_relay->enabled || !upgrade_relay || !upgrade_relay->enabled) {
        snprintf(err, errcap, "upgrade relays not available");
        return -1;
    }
    reset_port = resolve_port(reset_relay, platform, err, errcap);
    if (!reset_port) return -1;
    upgrade_port = resolve_port(upgrade_relay, platform, err, errcap);
    if (!upgrade_port) return -1;
    reset_idx = relay_find_index(cfg, reset_relay->name);
    upgrade_idx = relay_find_index(cfg, upgrade_relay->name);

    if (strcmp(reset_relay->name, upgrade_relay->name) == 0 &&
        board->reset.channel == board->upgrade_mode.channel) {
        snprintf(err, errcap, "upgrade_mode channel must differ from reset");
        return -1;
    }

    relay_action_idle_pulse(board->reset.polarity_inverted, &reset_idle, &reset_pulse);
    relay_action_idle_pulse(board->upgrade_mode.polarity_inverted, &upgrade_idle, &upgrade_pulse);

    if (set_logical(svc, reset_relay, reset_port, board->reset.channel, reset_pulse,
                    cache, reset_idx) != 0) {
        snprintf(err, errcap, "upgrade step1 failed");
        return -1;
    }
    if (set_logical(svc, upgrade_relay, upgrade_port, board->upgrade_mode.channel,
                    upgrade_pulse, cache, upgrade_idx) != 0) {
        snprintf(err, errcap, "upgrade step2 failed");
        return -1;
    }
    relay_executor_sleep(2.0);
    if (set_logical(svc, reset_relay, reset_port, board->reset.channel, reset_idle,
                    cache, reset_idx) != 0) {
        snprintf(err, errcap, "upgrade step3 failed");
        return -1;
    }
    relay_executor_sleep(2.0);
    if (set_logical(svc, upgrade_relay, upgrade_port, board->upgrade_mode.channel,
                    upgrade_idle, cache, upgrade_idx) != 0) {
        snprintf(err, errcap, "upgrade step4 failed");
        return -1;
    }
    return 0;
}

int relay_service_run_board_action(RelayService *svc, const BoardConfig *board,
                                   const AppConfig *cfg, const char *platform,
                                   const char *action, RelayStateCache *cache,
                                   char *err, size_t errcap) {
    const RelayConfig *reset_relay;
    const char *reset_port;
    int reset_idx;
    const BoardCustomAction *custom;

    if (!svc || !board || !cfg || !action) {
        snprintf(err, errcap, "invalid args");
        return -1;
    }
    if (!board->enabled) {
        snprintf(err, errcap, "board is disabled");
        return -1;
    }

    if (!relay_action_try_lock()) {
        snprintf(err, errcap, "another board action is in progress");
        return -1;
    }

    if (relay_str_eq_ci(action, "reset")) {
        if (!board->reset.enabled) {
            snprintf(err, errcap, "reset action is disabled");
            relay_action_unlock();
            return -1;
        }
        reset_relay = relay_find_by_name(cfg, board->reset.relay_name);
        if (!reset_relay || !reset_relay->enabled) {
            snprintf(err, errcap, "reset relay not available");
            relay_action_unlock();
            return -1;
        }
        reset_port = resolve_port(reset_relay, platform, err, errcap);
        if (!reset_port) {
            relay_action_unlock();
            return -1;
        }
        reset_idx = relay_find_index(cfg, reset_relay->name);
        if (run_pulse_sequence(svc, reset_relay, reset_port, board->reset.channel,
                               board->reset.hold_seconds, board->reset.polarity_inverted,
                               cache, reset_idx) != 0) {
            snprintf(err, errcap, "reset sequence failed");
            relay_action_unlock();
            return -1;
        }
        relay_action_unlock();
        return 0;
    }

    if (relay_str_eq_ci(action, "upgrade") || relay_str_eq_ci(action, "maskrom")) {
        if (!board->reset.enabled) {
            snprintf(err, errcap, "reset binding required for upgrade");
            relay_action_unlock();
            return -1;
        }
        if (run_upgrade_sequence(svc, board, cfg, platform, cache, err, errcap) != 0) {
            relay_action_unlock();
            return -1;
        }
        relay_action_unlock();
        return 0;
    }

    custom = relay_board_find_custom_action(board, action);
    if (custom) {
        if (run_custom_action(svc, custom, cfg, platform, cache, err, errcap) != 0) {
            relay_action_unlock();
            return -1;
        }
        relay_action_unlock();
        return 0;
    }

    relay_action_unlock();
    snprintf(err, errcap, "unsupported action");
    return -1;
}
