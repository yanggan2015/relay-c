#include "http_pages.h"
#include "relay_http.h"
#include "relay_action.h"
#include "relay_config.h"
#include "relay_types.h"
#include "util.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int host_is_local(const char *host) {
    if (!host || !host[0]) return 1;
    if (relay_str_eq_ci(host, "localhost")) return 1;
    if (strncmp(host, "127.", 4) == 0) return 1;
    if (strcmp(host, "::1") == 0) return 1;
    return 0;
}

/* Host → 对外 Base：Gateway :18081 用广告端口；直连本机用后端端口。 */
static void docs_base_url(RelayHttpServer *srv, struct mg_http_message *hm,
                          char *base, size_t cap, char *direct, size_t dcap) {
    char host[256];
    char hostname[256];
    char qhost[256];
    struct mg_str *hdr;
    AppConfig *cfg = relay_http_config(srv);
    int port = relay_http_port(srv);
    int adv = (cfg && cfg->advertise_http_port > 0) ? cfg->advertise_http_port : 18081;
    const char *bp = (cfg && cfg->base_path[0]) ? cfg->base_path : "/relay";
    int hp = -1;
    char *colon;

    snprintf(direct, dcap, "http://127.0.0.1:%d%s", port, bp);

    host[0] = qhost[0] = hostname[0] = '\0';
    mg_http_get_var(&hm->query, "host", qhost, sizeof(qhost));
    if (qhost[0]) {
        relay_str_copy(host, sizeof(host), qhost);
    } else {
        hdr = mg_http_get_header(hm, "Host");
        if (hdr && hdr->len > 0)
            snprintf(host, sizeof(host), "%.*s", (int)hdr->len, hdr->buf);
    }

    if (!host[0]) {
        relay_str_copy(base, cap, direct);
        return;
    }

    relay_str_copy(hostname, sizeof(hostname), host);
    colon = strrchr(hostname, ':');
    if (colon && strchr(hostname, ']') == NULL) {
        *colon = '\0';
        hp = atoi(colon + 1);
    }

    if (host_is_local(hostname)) {
        if (hp == adv || hp == 18081)
            snprintf(base, cap, "http://127.0.0.1:%d%s", adv, bp);
        else
            snprintf(base, cap, "http://127.0.0.1:%d%s", port, bp);
        return;
    }

    if (hp == adv || hp == 18081 || hp < 0)
        snprintf(base, cap, "http://%s:%d%s", hostname, adv, bp);
    else
        snprintf(base, cap, "http://%s:%d%s", hostname, hp, bp);
}

/* 说明紧挨命令；命令后空一行；不用代码围栏。 */
static size_t md_curl(char *md, size_t cap, size_t pos,
                      const char *label, const char *base, const char *path) {
    if (label && label[0])
        pos += (size_t)snprintf(md + pos, cap - pos, "**%s**\n", label);
    pos += (size_t)snprintf(md + pos, cap - pos, "curl.exe -s \"%s%s\"\n\n", base, path);
    return pos;
}

static size_t docs_append_board(char *md, size_t cap, size_t pos, const char *base,
                                const BoardConfig *b) {
    char path[512];
    char label[192];
    int j;

    pos += (size_t)snprintf(md + pos, cap - pos, "### 板卡 `%s`\n\n", b->name);
    if (!b->enabled) {
        pos += (size_t)snprintf(md + pos, cap - pos, "> 配置中 `enabled=false`，动作可能被拒绝。\n\n");
    }

    snprintf(path, sizeof(path), "/api/boards/%s/action?action=reset", b->name);
    pos = md_curl(md, cap, pos, "复位", base, path);

    snprintf(path, sizeof(path), "/api/boards/%s/action?action=on", b->name);
    pos = md_curl(md, cap, pos, "打开", base, path);

    snprintf(path, sizeof(path), "/api/boards/%s/action?action=off", b->name);
    pos = md_curl(md, cap, pos, "关闭", base, path);

    snprintf(path, sizeof(path), "/api/boards/%s/state", b->name);
    pos = md_curl(md, cap, pos, "获取状态", base, path);

    if (b->has_upgrade_mode && b->upgrade_mode.enabled && b->upgrade_mode.relay_name[0]) {
        snprintf(path, sizeof(path), "/api/boards/%s/action?action=upgrade", b->name);
        pos = md_curl(md, cap, pos, "进入烧写模式", base, path);
    }

    for (j = 0; j < b->n_custom_actions; ++j) {
        const BoardCustomAction *a = &b->custom_actions[j];
        if (!a->enabled) continue;
        snprintf(label, sizeof(label), "%s", a->label[0] ? a->label : a->name);
        snprintf(path, sizeof(path), "/api/boards/%s/action?action=%s", b->name, a->name);
        pos = md_curl(md, cap, pos, label, base, path);
    }

    return pos;
}

static size_t docs_build_markdown(AppConfig *cfg, const char *base, const char *direct,
                                  char *md, size_t cap) {
    char path[512];
    char label[160];
    size_t pos = 0;
    int i, ch;

    pos += (size_t)snprintf(md + pos, cap - pos,
        "# EADK Relay — Agent 操作手册\n\n"
        "**重点：按板卡复制下方操控指令。** PowerShell 用 curl.exe；写操作均为 GET。\n\n"
        "- 机器清单: %s/api/help\n"
        "- WebUI: %s/\n"
        "- Agent 接口: %s/help\n"
        "- Base: `%s`（局域网经 Gateway 用主机 IP:18081）\n"
        "- 直连: `%s`\n"
        "- 版本 %s · %s\n\n"
        "---\n\n"
        "## 1. 板卡操控指令（优先看这里）\n\n",
        base, base, base, base, direct, RELAY_VERSION, cfg->platform);

    if (cfg->n_boards <= 0) {
        pos += (size_t)snprintf(md + pos, cap - pos,
            "当前配置无 boards。先 `GET /api/boards` 或编辑 relay_config.json / boards.json。\n\n");
    } else {
        pos += (size_t)snprintf(md + pos, cap - pos,
            "共 %d 块板。每块标准指令：复位 / 打开 / 关闭 / 获取状态；有烧写模式时额外给出。\n\n",
            cfg->n_boards);
        for (i = 0; i < cfg->n_boards; ++i)
            pos = docs_append_board(md, cap, pos, base, &cfg->boards[i]);
    }

    pos += (size_t)snprintf(md + pos, cap - pos,
        "---\n\n"
        "## 2. 继电器直控（不经板级 action）\n\n");

    if (cfg->n_relays > 0) {
        for (i = 0; i < cfg->n_relays; ++i) {
            const RelayConfig *r = &cfg->relays[i];
            pos += (size_t)snprintf(md + pos, cap - pos,
                "### 继电器 `%s`（%d 通道）\n\n", r->name, r->relay_channels);
            for (ch = 1; ch <= r->relay_channels && ch <= 8; ++ch) {
                snprintf(label, sizeof(label), "ch%d ON", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/relay?channel=%d&state=true", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
                snprintf(label, sizeof(label), "ch%d OFF", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/relay?channel=%d&state=false", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
                snprintf(label, sizeof(label), "ch%d state", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/state?channel=%d", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
            }
            snprintf(path, sizeof(path), "/api/relays/%s/state", r->name);
            pos = md_curl(md, cap, pos, "全部通道 state", base, path);
        }
    } else {
        pos += (size_t)snprintf(md + pos, cap - pos, "当前配置无 relays。\n\n");
    }

    pos += (size_t)snprintf(md + pos, cap - pos,
        "---\n\n"
        "## 3. 列表接口\n\n");
    pos = md_curl(md, cap, pos, "板列表 /api/boards", base, "/api/boards");
    pos = md_curl(md, cap, pos, "板能力 /api/boards/support", base, "/api/boards/support");
    pos = md_curl(md, cap, pos, "继电器列表 /api/relays", base, "/api/relays");
    pos = md_curl(md, cap, pos, "health", base, "/health");

    pos += (size_t)snprintf(md + pos, cap - pos,
        "---\n\n"
        "## 4. 接口速查（与 /api/help 一致）\n\n"
        "- `GET /health`\n"
        "- `GET /api/help`\n"
        "- `GET /help`\n"
        "- `GET /api/boards`\n"
        "- `GET /api/boards/support`\n"
        "- `GET /api/boards/{name}/action?action=` — reset|upgrade|自定义\n"
        "- `GET /api/boards/{name}/state`\n"
        "- `GET /api/relays`\n"
        "- `GET /api/relays/{name}/relay?channel=&state=`\n"
        "- `GET /api/relays/{name}/state?channel=`\n"
        "- `GET /api/relays/{name}/raw?data=`\n"
        "- `GET /api/serial/ports`\n"
        "- `GET /api/config/reload`\n");

    return pos;
}

void relay_http_page_docs(struct mg_connection *c, RelayHttpServer *srv,
                          struct mg_http_message *hm) {
    char *md;
    char base[192];
    char direct[128];
    size_t md_cap = 262144;
    size_t md_len;
    AppConfig *cfg = relay_http_config(srv);

    md = (char *)malloc(md_cap);
    if (!md || !cfg) {
        mg_http_reply(c, 500, "", "oom");
        free(md);
        return;
    }

    docs_base_url(srv, hm, base, sizeof(base), direct, sizeof(direct));
    md_len = docs_build_markdown(cfg, base, direct, md, md_cap);
    (void)md_len;

    mg_http_reply(c, 200,
                  "Content-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s", md);
    free(md);
}
