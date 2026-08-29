#include "relay_config.h"
#include "relay_protocol.h"
#include "relay_action.h"
#include "relay_service.h"
#include "relay_serial.h"
#include "relay_lock.h"
#include "util.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int test_protocol(void) {
    RelayCommand cmd;
    uint8_t buf[16];
    size_t len;
    int st;

    if (relay_build_a0_set(&cmd, 1, 1) != 0) return fail("a0 set");
    if (cmd.len != 4 || cmd.bytes[0] != 0xA0) return fail("a0 set bytes");

    if (relay_parse_hex_payload("A0 01 01 A2", buf, sizeof(buf), &len) != 0) return fail("hex parse");
    if (len != 4) return fail("hex len");

    if (relay_parse_str_payload("on\\r\\n", buf, sizeof(buf), &len) != 0) return fail("str parse");
    if (len != 4 || buf[2] != '\r') return fail("str content");

    if (relay_parse_a0_response((const uint8_t[]){0xA0, 1, 1, 0}, 4, 1, &st) != 0) return fail("a0 resp");
    if (st != 1) return fail("a0 state");

    return 0;
}

static int test_action_semantics(void) {
    int idle, pulse;
    RelayConfig relay;

    relay_action_idle_pulse(0, &idle, &pulse);
    if (idle != 1 || pulse != 0) return fail("default idle=ON pulse=OFF");

    relay_action_idle_pulse(1, &idle, &pulse);
    if (idle != 0 || pulse != 1) return fail("inverted idle=OFF pulse=ON");

    memset(&relay, 0, sizeof(relay));
    if (relay_logical_to_wire(&relay, 1) != 1) return fail("wire 1");
    if (relay_logical_to_wire(&relay, 0) != 0) return fail("wire 0");
    relay.io_inverted = 1;
    if (relay_logical_to_wire(&relay, 1) != 0) return fail("io inv on");
    if (relay_logical_to_wire(&relay, 0) != 1) return fail("io inv off");

    if (relay_action_mode_parse("hold_on") != ACTION_MODE_HOLD_ON) return fail("mode hold_on");
    if (relay_action_mode_parse("pulse") != ACTION_MODE_PULSE) return fail("mode pulse");

    return 0;
}

static int test_config(void) {
    AppConfig cfg;
    char err[256];

    if (relay_config_load(&cfg, "boards.json") != 0) return fail("load config");
    if (cfg.n_relays < 1) return fail("no relays");
    if (cfg.server_port != 18053) return fail("port not 18053");
    if (cfg.n_boards < 1) return fail("no boards");
    if (!cfg.boards[0].has_upgrade_mode) return fail("upgrade_mode missing");
    if (cfg.boards[0].n_custom_actions < 1) return fail("custom actions missing");
    if (relay_config_validate_board(&cfg.boards[0], &cfg, err, sizeof(err)) != 0)
        return fail(err);
    return 0;
}

static int test_service_dry(void) {
    AppConfig cfg;
    RelayStateCache cache;
    RelayExecutor *ex;
    RelayService *svc;
    char err[256];
    int st;

    memset(&cache, 0, sizeof(cache));
    relay_executor_set_dry_run(1);
    if (relay_config_load(&cfg, "boards.json") != 0) return fail("load");

    ex = relay_executor_create();
    svc = relay_service_create(ex);
    if (!svc) return fail("service");

    if (relay_service_run_relay_direct(svc, &cfg.relays[0], cfg.platform, 1, 1,
                                       &cache, 0, err, sizeof(err)) != 0)
        return fail(err);

    if (relay_service_query_state(svc, &cfg.relays[0], cfg.platform, 1, &st,
                                  &cache, 0, err, sizeof(err)) != 0)
        return fail(err);

    if (relay_service_run_board_action(svc, &cfg.boards[0], &cfg, cfg.platform,
                                       "reset", &cache, err, sizeof(err)) != 0)
        return fail(err);

    if (relay_service_run_board_action(svc, &cfg.boards[0], &cfg, cfg.platform,
                                       "upgrade", &cache, err, sizeof(err)) != 0)
        return fail(err);

    if (relay_service_run_board_action(svc, &cfg.boards[0], &cfg, cfg.platform,
                                       "power_led", &cache, err, sizeof(err)) != 0)
        return fail(err);

    if (relay_service_run_board_action(svc, &cfg.boards[0], &cfg, cfg.platform,
                                       "custom_pulse", &cache, err, sizeof(err)) != 0)
        return fail(err);

    relay_service_free(svc);
    relay_executor_free(ex);
    relay_executor_set_dry_run(0);
    return 0;
}

static int test_config_roundtrip(void) {
    AppConfig cfg;
    char err[256];
    cJSON *fields;

    if (relay_config_load(&cfg, "boards.json") != 0) return fail("load");
    fields = cJSON_CreateObject();
    cJSON_AddStringToObject(fields, "windows_port", "COM99");
    cJSON_AddNumberToObject(fields, "relay_channels", 1);
    if (relay_config_relay_create(&cfg, "tmp_test_relay", fields, err, sizeof(err)) != 0) {
        cJSON_Delete(fields);
        return fail(err);
    }
    cJSON_Delete(fields);
    if (relay_config_relay_delete(&cfg, "tmp_test_relay", err, sizeof(err)) != 0)
        return fail(err);
    if (relay_config_reload(&cfg) != 0) return fail("reload");
    return 0;
}

int main(void) {
    int rc = 0;
    relay_lock_init();
    printf("test_protocol...\n");
    if (test_protocol()) rc = 1;
    printf("test_action_semantics...\n");
    if (test_action_semantics()) rc = 1;
    printf("test_config...\n");
    if (test_config()) rc = 1;
    printf("test_config_roundtrip...\n");
    if (test_config_roundtrip()) rc = 1;
    printf("test_service_dry...\n");
    if (test_service_dry()) rc = 1;
    relay_lock_shutdown();
    if (rc == 0) printf("ALL OK\n");
    return rc;
}
