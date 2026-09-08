#ifndef RELAY_CLI_HTTP_H
#define RELAY_CLI_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tiny HTTP GET to 127.0.0.1:port + path_and_query (must start with '/').
 * Returns malloc'd response body (caller frees), or NULL on failure.
 * If out_status non-NULL, stores HTTP status (or 0 on transport error).
 */
char *relay_cli_http_get(int port, const char *path_and_query, int *out_status);

#ifdef __cplusplus
}
#endif

#endif
