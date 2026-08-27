#ifndef HTTP_PAGES_H
#define HTTP_PAGES_H

struct mg_connection;
typedef struct RelayHttpServer RelayHttpServer;

const char *relay_page_styles(void);
const char *relay_page_head(const char *title);
const char *relay_page_tail(void);
const char *relay_page_nav(const char *active);
void relay_http_page_config(struct mg_connection *c, RelayHttpServer *srv);
void relay_http_page_control(struct mg_connection *c, RelayHttpServer *srv);
void relay_http_page_docs(struct mg_connection *c, RelayHttpServer *srv);

#endif
