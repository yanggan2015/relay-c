#include "relay_startup.h"
#include "relay_service.h"
#include "util.h"

int relay_startup_init_relays(RelayService *svc, AppConfig *cfg, RelayStateCache *cache) {
    int i, ch;
    char err[256];

    if (!svc || !cfg) return -1;
    for (i = 0; i < cfg->n_relays; ++i) {
        RelayConfig *r = &cfg->relays[i];
        int idx = i;
        if (!r->enabled || r->startup_init == RELAY_STARTUP_SKIP) continue;
        for (ch = 1; ch <= r->relay_channels; ++ch) {
            if (relay_service_run_relay_direct(svc, r, cfg->platform, ch, r->startup_init,
                                               cache, idx, err, sizeof(err)) != 0) {
                relay_log("[startup] %s ch%d init failed: %s", r->name, ch, err);
            }
        }
        relay_log("[startup] %s channels initialized to %s", r->name,
                  r->startup_init ? "ON" : "OFF");
    }
    return 0;
}
