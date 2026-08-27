#ifndef RELAY_HTTP_API_H
#define RELAY_HTTP_API_H

#include "relay_http.h"
#include "relay_types.h"

struct mg_connection;
struct mg_http_message;

int relay_http_api_handle_config(struct mg_connection *c, RelayHttpServer *srv,
                                 struct mg_http_message *hm);
int relay_http_reload_config(RelayHttpServer *srv, char *err, size_t errcap);

#endif
