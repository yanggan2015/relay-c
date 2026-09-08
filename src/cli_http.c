#include "cli_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET cli_sock_t;
#define CLI_INVALID INVALID_SOCKET
#define cli_close   closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int cli_sock_t;
#define CLI_INVALID (-1)
#define cli_close   close
#endif

char *relay_cli_http_get(int port, const char *path_and_query, int *out_status) {
    cli_sock_t s = CLI_INVALID;
    struct sockaddr_in addr;
    char req[2048];
    char *resp = NULL;
    size_t cap = 0, len = 0;
    char chunk[4096];
    int n, status = 0;
    char *body;
    const char *path = path_and_query && path_and_query[0] ? path_and_query : "/";

    if (out_status) *out_status = 0;
    if (port <= 0 || port > 65535) return NULL;

#ifdef _WIN32
    {
        static int wsa_once = 0;
        if (!wsa_once) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return NULL;
            wsa_once = 1;
        }
    }
#endif

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == CLI_INVALID) return NULL;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        cli_close(s);
        return NULL;
    }

    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", path);
    if (send(s, req, (int)strlen(req), 0) < 0) {
        cli_close(s);
        return NULL;
    }

    for (;;) {
        n = (int)recv(s, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        if (len + (size_t)n + 1 > cap) {
            size_t ncap = cap ? cap * 2 : 8192;
            char *nr;
            while (ncap < len + (size_t)n + 1) ncap *= 2;
            nr = (char *)realloc(resp, ncap);
            if (!nr) {
                free(resp);
                cli_close(s);
                return NULL;
            }
            resp = nr;
            cap = ncap;
        }
        memcpy(resp + len, chunk, (size_t)n);
        len += (size_t)n;
        resp[len] = '\0';
    }
    cli_close(s);

    if (!resp || len == 0) {
        free(resp);
        return NULL;
    }

    if (strncmp(resp, "HTTP/", 5) == 0) {
        const char *sp = strchr(resp, ' ');
        if (sp) status = atoi(sp + 1);
    }
    if (out_status) *out_status = status;

    body = strstr(resp, "\r\n\r\n");
    if (body) {
        size_t blen = strlen(body + 4);
        char *out = (char *)malloc(blen + 1);
        if (!out) {
            free(resp);
            return NULL;
        }
        memcpy(out, body + 4, blen + 1);
        free(resp);
        return out;
    }
    return resp;
}
