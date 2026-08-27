#include "http_pages.h"
#include "relay_config.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void relay_http_page_docs(struct mg_connection *c, RelayHttpServer *srv) {
    char *html;
    size_t cap = 65536;
    size_t pos = 0;
    int i;

    html = (char *)malloc(cap);
    if (!html) {
        mg_http_reply(c, 500, "", "oom");
        return;
    }

    pos += (size_t)snprintf(html + pos, cap - pos, "%s%s",
        relay_page_head("Relay-C Docs"), relay_page_nav("docs"));
    pos += (size_t)snprintf(html + pos, cap - pos,
        "<section>"
        "<h2>Web 页面</h2><div class=\"doc-grid\">"
        "<article class=\"card\"><h3>Control</h3><pre>GET /</pre></article>"
        "<article class=\"card\"><h3>Config</h3><pre>GET /config</pre></article>"
        "<article class=\"card\"><h3>Docs</h3><pre>GET /docs</pre></article>"
        "<article class=\"card\"><h3>Health</h3><pre>GET /api/health</pre></article>"
        "</div>"
        "<h2>串口 / 配置</h2><div class=\"doc-grid\">"
        "<article class=\"card\"><h3>探测串口</h3><pre>GET /api/serial/ports</pre></article>"
        "<article class=\"card\"><h3>热重载</h3><pre>GET /api/config/reload</pre></article>"
        "<article class=\"card\"><h3>Relay CRUD</h3>"
        "<pre>GET /api/config/relay/create?...\nGET /api/config/relay/update?...\nGET /api/config/relay/delete?name=</pre></article>"
        "<article class=\"card\"><h3>Board CRUD</h3>"
        "<pre>GET /api/config/board/create?...\nGET /api/config/board/update?...\nGET /api/config/board/delete?name=</pre></article>"
        "<article class=\"card\"><h3>Server 端口</h3><pre>GET /api/config/server/set?port=</pre></article>"
        "</div>");

    if (srv->cfg->n_relays > 0) {
        pos += (size_t)snprintf(html + pos, cap - pos, "<h2>Relay API</h2><div class=\"doc-grid\">");
        for (i = 0; i < srv->cfg->n_relays; ++i) {
            const RelayConfig *r = &srv->cfg->relays[i];
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<article class=\"card\"><h3>%s</h3>"
                "<pre>/api/relays/%s/relay?channel=1&amp;state=true</pre>"
                "<pre>/api/relays/%s/state?channel=1</pre>"
                "<pre>/api/relays/%s/raw?format=hex&amp;data=A0%%2001%%2001%%20A2</pre></article>",
                r->name, r->name, r->name, r->name);
        }
        pos += (size_t)snprintf(html + pos, cap - pos, "</div>");
    }

    if (srv->cfg->n_boards > 0) {
        pos += (size_t)snprintf(html + pos, cap - pos, "<h2>Board API</h2><div class=\"doc-grid\">");
        for (i = 0; i < srv->cfg->n_boards; ++i) {
            const BoardConfig *b = &srv->cfg->boards[i];
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<article class=\"card\"><h3>%s</h3>"
                "<pre>/api/boards/%s/action?action=reset</pre>",
                b->name, b->name);
            if (b->has_upgrade_mode && b->upgrade_mode.enabled)
                pos += (size_t)snprintf(html + pos, cap - pos,
                    "<pre>/api/boards/%s/action?action=upgrade</pre>", b->name);
            {
                int j;
                for (j = 0; j < b->n_custom_actions; ++j) {
                    const BoardCustomAction *a = &b->custom_actions[j];
                    if (!a->enabled) continue;
                    pos += (size_t)snprintf(html + pos, cap - pos,
                        "<pre>/api/boards/%s/action?action=%s</pre>",
                        b->name, a->name);
                }
            }
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<pre>/api/boards/%s/state</pre></article>", b->name);
        }
        pos += (size_t)snprintf(html + pos, cap - pos, "</div>");
    }

    pos += (size_t)snprintf(html + pos, cap - pos, "</section>%s", relay_page_tail());

    mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n", "%s", html);
    free(html);
}
