#include "http_pages.h"
#include "relay_http.h"
#include "relay_config.h"
#include "util.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void relay_http_page_control(struct mg_connection *c, RelayHttpServer *srv) {
    char *html;
    size_t cap = 65536;
    size_t pos = 0;
    int i;
    int http_port;
    AppConfig *cfg = relay_http_config(srv);

    html = (char *)malloc(cap);
    if (!html || !cfg) {
        mg_http_reply(c, 500, "", "oom");
        free(html);
        return;
    }

    http_port = relay_http_port(srv);
    pos += (size_t)snprintf(html + pos, cap - pos, "%s%s",
        relay_page_head("relay-c 控制"), relay_page_nav("control"));

    pos += (size_t)snprintf(html + pos, cap - pos,
        "<div class=\"kpi\">"
        "<div class=\"cell\"><div class=\"k\">继电器</div><div class=\"v\">%d</div></div>"
        "<div class=\"cell\"><div class=\"k\">板卡</div><div class=\"v\">%d</div></div>"
        "<div class=\"cell\"><div class=\"k\">网页端口</div><div class=\"v\">%d</div></div>"
        "<div class=\"cell\"><div class=\"k\">平台</div><div class=\"v\">%s</div></div>"
        "</div>",
        cfg->n_relays, cfg->n_boards, http_port,
        cfg->platform[0] ? cfg->platform : "-");

    pos += (size_t)snprintf(html + pos, cap - pos,
        "<section class=\"config-cols\">"
        "<div class=\"config-col\">"
        "<h2>继电器</h2><p class=\"sub\">单通道开 / 关 / 查询</p>");

    for (i = 0; i < cfg->n_relays; ++i) {
        const RelayConfig *r = &cfg->relays[i];
        const char *port = relay_resolved_port(r->port, r->linux_port, r->windows_port,
                                               cfg->platform);
        int ch;
        pos += (size_t)snprintf(html + pos, cap - pos,
            "<article class=\"card%s\">"
            "<h3>%s%s</h3>"
            "<div class=\"meta-row\">"
            "<span class=\"chip\">%d 路</span>"
            "<span class=\"chip\">%s</span>"
            "<span class=\"chip\">%d baud</span>%s</div>",
            r->enabled ? "" : " disabled", r->name,
            r->enabled ? "" : "（已禁用）", r->relay_channels,
            port[0] ? port : "未设串口", r->baudrate,
            r->io_inverted ? "<span class=\"chip\">io_inv</span>" : "");
        pos += (size_t)snprintf(html + pos, cap - pos, "<div class=\"ch-row\">");
        for (ch = 1; ch <= r->relay_channels; ++ch) {
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<div class=\"ch-group\">"
                "<span class=\"ch-label\">CH%d</span>"
                "<button class=\"primary\" onclick=\"relaySet('%s',%d,true)\">开</button>"
                "<button class=\"ghost\" onclick=\"relaySet('%s',%d,false)\">关</button>"
                "<button class=\"ghost\" onclick=\"relayState('%s',%d)\">?</button>"
                "</div>",
                ch, r->name, ch, r->name, ch, r->name, ch);
        }
        pos += (size_t)snprintf(html + pos, cap - pos,
            "</div><div id=\"st-%s\" class=\"status\"></div></article>", r->name);
    }

    pos += (size_t)snprintf(html + pos, cap - pos,
        "</div><div class=\"config-col\">"
        "<h2>板卡</h2><p class=\"sub\">复位 / 升级 / 自定义动作</p>");

    for (i = 0; i < cfg->n_boards; ++i) {
        const BoardConfig *b = &cfg->boards[i];
        pos += (size_t)snprintf(html + pos, cap - pos,
            "<article class=\"card%s\">"
            "<h3>%s</h3>"
            "<div class=\"meta-row\">"
            "<span class=\"chip\">复位 %s · ch%d</span>",
            b->enabled ? "" : " disabled", b->name,
            b->reset.relay_name, b->reset.channel);
        if (b->has_upgrade_mode && b->upgrade_mode.enabled)
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<span class=\"chip\">升级 %s · ch%d</span>",
                b->upgrade_mode.relay_name, b->upgrade_mode.channel);
        pos += (size_t)snprintf(html + pos, cap - pos, "</div><div class=\"btn-row\">"
            "<button class=\"primary\" onclick=\"boardAct('%s','reset')\">复位</button>",
            b->name);
        if (b->has_upgrade_mode && b->upgrade_mode.enabled)
            pos += (size_t)snprintf(html + pos, cap - pos,
                "<button class=\"warn\" onclick=\"boardAct('%s','upgrade')\">升级</button>",
                b->name);
        {
            int j;
            for (j = 0; j < b->n_custom_actions; ++j) {
                const BoardCustomAction *a = &b->custom_actions[j];
                if (!a->enabled) continue;
                pos += (size_t)snprintf(html + pos, cap - pos,
                    "<button class=\"ghost\" onclick=\"boardAct('%s','%s')\">%s</button>",
                    b->name, a->name, a->label[0] ? a->label : a->name);
            }
        }
        pos += (size_t)snprintf(html + pos, cap - pos,
            "<button class=\"ghost\" onclick=\"boardState('%s')\">状态</button>"
            "</div><div id=\"bst-%s\" class=\"status\"></div></article>", b->name, b->name);
    }

    pos += (size_t)snprintf(html + pos, cap - pos,
        "</div></section>"
        "<script>"
        "async function j(u){const r=await fetch(u);return[r,await r.json()];}"
        "function st(id,ok,t){const e=document.getElementById(id);e.className=ok?'status ok':'status err';e.textContent=t;}"
        "async function relaySet(n,ch,s){const[r,d]=await j(`/api/relays/${encodeURIComponent(n)}/relay?channel=${ch}&state=${s}`);st('st-'+n,r.ok,r.ok?`CH${ch} => ${s?'开':'关'}`:(d.error||'fail'));}"
        "async function relayState(n,ch){const[r,d]=await j(`/api/relays/${encodeURIComponent(n)}/state?channel=${ch}`);st('st-'+n,r.ok,r.ok?`CH${ch} => ${d.state?'开':'关'}`:(d.error||'fail'));}"
        "async function boardAct(n,a){const[r,d]=await j(`/api/boards/${encodeURIComponent(n)}/action?action=${a}`);st('bst-'+n,r.ok,r.ok?`${a} ok`:(d.error||'fail'));}"
        "async function boardState(n){const[r,d]=await j(`/api/boards/${encodeURIComponent(n)}/state`);st('bst-'+n,r.ok,r.ok?JSON.stringify(d.states):(d.error||'fail'));}"
        "</script>%s", relay_page_tail());

    mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n", "%s", html);
    free(html);
}
