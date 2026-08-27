#ifndef RELAY_HTTP_H
#define RELAY_HTTP_H

#include "relay_types.h"
#include "relay_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RelayHttpServer RelayHttpServer;

RelayHttpServer *relay_http_create(AppConfig *cfg, RelayStateCache *cache);
void             relay_http_free(RelayHttpServer *srv);
int              relay_http_start(RelayHttpServer *srv, int port);
void             relay_http_stop(RelayHttpServer *srv);
int              relay_http_poll(RelayHttpServer *srv, int ms);
RelayService    *relay_http_service(RelayHttpServer *srv);
AppConfig       *relay_http_config(RelayHttpServer *srv);
int              relay_http_port(RelayHttpServer *srv);

#ifdef __cplusplus
}
#endif

#endif
