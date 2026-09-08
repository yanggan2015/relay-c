#ifndef EADK_DISCOVERY_H
#define EADK_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

/* UDP LAN discovery responder for EmbeddedAIDevelopmentKit tools. */
#define EADK_DISCOVERY_UDP_PORT 18071
#define EADK_DISCOVERY_MAGIC    "EADK1"

typedef struct EadkDiscovery EadkDiscovery;

/* Start listener thread. config_path re-read each query for discoverable/display_name. */
EadkDiscovery *eadk_discovery_start(const char *config_path, int http_port,
                                    const char *tool, const char *version);
void eadk_discovery_set(EadkDiscovery *d, const char *config_path, int http_port);
void eadk_discovery_stop(EadkDiscovery *d);

#ifdef __cplusplus
}
#endif

#endif
