#ifndef RELAY_STARTUP_H
#define RELAY_STARTUP_H

#include "relay_config.h"
#include "relay_service.h"
#include "relay_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 按 relay.startup_init 初始化各通道逻辑电平；-1=跳过 */
int relay_startup_init_relays(RelayService *svc, AppConfig *cfg, RelayStateCache *cache);

#ifdef __cplusplus
}
#endif

#endif
