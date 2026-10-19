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

static size_t docs_build_markdown(AppConfig *cfg, const char *base, const char *direct,
                                  char *md, size_t cap) {
    char path[512];
    char label[160];
    size_t pos = 0;
    int i, ch;
    const char *ex_board = "BOARD";
    const char *ex_relay = "RELAY";

    if (cfg->n_boards > 0)
        ex_board = cfg->boards[0].name;
    if (cfg->n_relays > 0)
        ex_relay = cfg->relays[0].name;

    pos += (size_t)snprintf(md + pos, cap - pos,
        "# EADK Relay — Agent 操作手册\n\n"
        "先看约定与 boards/relays 字段，再复制下面的 curl（说明紧挨命令，不同命令之间空一行）。\n"
        "PowerShell / Agent 请用 curl.exe。\n\n"
        "- 机器清单: %s/api/help\n"
        "- WebUI: %s/\n"
        "- Agent 接口: %s/help\n\n"
        "---\n\n"
        "## 1. 约定\n\n"
        "- **Base**: `%s`（局域网经 Gateway 时用主机 IP:18081，勿写死 127.0.0.1 给远端 Agent）\n"
        "- 本机直连后端: `%s`（不经 Gateway）\n"
        "- 板名 / 继电器名见 `GET /api/boards`、`GET /api/relays`；下文示例用 `%s` / `%s`\n"
        "- 写操作均为 **GET**（query）；会阻塞到动作结束，返回 JSON\n"
        "- 版本 %s · platform %s\n\n"
        "---\n\n"
        "## 2. 列表返回什么\n\n"
        "### 2.1 GET /api/boards\n\n",
        base, base, base, base, direct, ex_board, ex_relay,
        RELAY_VERSION, cfg->platform);

    pos = md_curl(md, cap, pos, NULL, base, "/api/boards");
    pos += (size_t)snprintf(md + pos, cap - pos,
        "JSON 顶层:\n\n"
        "- `boards` — 对象，key 为板名\n\n"
        "每个 board 常用字段:\n\n"
        "- `reset` — 复位继电器名 / 通道 / hold 秒\n"
        "- `upgrade_mode` — 可选；镜像升级/烧录模式通道\n"
        "- `custom_actions` — 自定义动作列表\n\n"
        "### 2.2 GET /api/relays\n\n");
    pos = md_curl(md, cap, pos, NULL, base, "/api/relays");
    pos += (size_t)snprintf(md + pos, cap - pos,
        "JSON 顶层:\n\n"
        "- `relays` — 对象，key 为继电器名；含通道数、串口等\n\n"
        "---\n\n"
        "## 3. 典型操作示例\n\n"
        "把板名 / 继电器名换成真实配置。\n\n");

    if (cfg->n_boards > 0) {
        pos += (size_t)snprintf(md + pos, cap - pos, "### 3.1 板级动作\n\n");
        for (i = 0; i < cfg->n_boards; ++i) {
            const BoardConfig *b = &cfg->boards[i];
            pos += (size_t)snprintf(md + pos, cap - pos, "#### board: `%s`\n\n", b->name);

            snprintf(path, sizeof(path), "/api/boards/%s/action?action=reset", b->name);
            pos = md_curl(md, cap, pos, "复位 reset", base, path);

            if (b->has_upgrade_mode && b->upgrade_mode.enabled) {
                snprintf(path, sizeof(path), "/api/boards/%s/action?action=upgrade", b->name);
                pos = md_curl(md, cap, pos, "升级模式 upgrade", base, path);
            }
            {
                int j;
                for (j = 0; j < b->n_custom_actions; ++j) {
                    const BoardCustomAction *a = &b->custom_actions[j];
                    if (!a->enabled) continue;
                    snprintf(label, sizeof(label), "%s (%s)",
                             a->label[0] ? a->label : a->name, a->name);
                    snprintf(path, sizeof(path),
                             "/api/boards/%s/action?action=%s", b->name, a->name);
                    pos = md_curl(md, cap, pos, label, base, path);
                }
            }
            snprintf(path, sizeof(path), "/api/boards/%s/state", b->name);
            pos = md_curl(md, cap, pos, "板状态 state", base, path);
        }
    } else {
        pos += (size_t)snprintf(md + pos, cap - pos,
            "### 3.1 板级动作（无配置时占位）\n\n");
        snprintf(path, sizeof(path), "/api/boards/%s/action?action=reset", ex_board);
        pos = md_curl(md, cap, pos, "复位 reset", base, path);
        snprintf(path, sizeof(path), "/api/boards/%s/state", ex_board);
        pos = md_curl(md, cap, pos, "板状态 state", base, path);
    }

    if (cfg->n_relays > 0) {
        pos += (size_t)snprintf(md + pos, cap - pos, "### 3.2 继电器通道\n\n");
        for (i = 0; i < cfg->n_relays; ++i) {
            const RelayConfig *r = &cfg->relays[i];
            pos += (size_t)snprintf(md + pos, cap - pos,
                "#### relay: `%s` (%d channels)\n\n", r->name, r->relay_channels);
            for (ch = 1; ch <= r->relay_channels && ch <= 4; ++ch) {
                snprintf(label, sizeof(label), "ch%d on", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/relay?channel=%d&state=true", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
                snprintf(label, sizeof(label), "ch%d off", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/relay?channel=%d&state=false", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
                snprintf(label, sizeof(label), "ch%d state", ch);
                snprintf(path, sizeof(path),
                         "/api/relays/%s/state?channel=%d", r->name, ch);
                pos = md_curl(md, cap, pos, label, base, path);
            }
            if (r->relay_channels > 4) {
                pos += (size_t)snprintf(md + pos, cap - pos,
                    "其余通道同理：`/api/relays/%s/relay?channel=N&state=true|false`\n\n",
                    r->name);
            }
            snprintf(path, sizeof(path), "/api/relays/%s/state", r->name);
            pos = md_curl(md, cap, pos, "全部通道 state", base, path);
        }
    } else {
        pos += (size_t)snprintf(md + pos, cap - pos,
            "### 3.2 继电器通道（无配置时占位）\n\n");
        snprintf(path, sizeof(path),
                 "/api/relays/%s/relay?channel=1&state=true", ex_relay);
        pos = md_curl(md, cap, pos, "ch1 on", base, path);
        snprintf(path, sizeof(path),
                 "/api/relays/%s/relay?channel=1&state=false", ex_relay);
        pos = md_curl(md, cap, pos, "ch1 off", base, path);
        snprintf(path, sizeof(path), "/api/relays/%s/state?channel=1", ex_relay);
        pos = md_curl(md, cap, pos, "ch1 state", base, path);
    }

    pos += (size_t)snprintf(md + pos, cap - pos, "### 3.3 存活\n\n");
    pos = md_curl(md, cap, pos, "health", base, "/health");

    pos += (size_t)snprintf(md + pos, cap - pos,
        "---\n\n"
        "## 4. 接口速查（与 /api/help 一致）\n\n"
        "- `GET /health` — 存活；`tool=relay`\n"
        "- `GET /api/help` — 机器可读 operations\n"
        "- `GET /help` — Agent 接口（text/plain Markdown）；`/docs` 为其别名\n"
        "- `GET /api/boards` — 板列表\n"
        "- `GET /api/boards/support` — 板能力\n"
        "- `GET /api/boards/{name}/action?action=` — reset|upgrade|自定义\n"
        "- `GET /api/boards/{name}/state` — 板状态\n"
        "- `GET /api/relays` — 继电器列表\n"
        "- `GET /api/relays/{name}/relay?channel=&state=` — 通断\n"
        "- `GET /api/relays/{name}/state?channel=` — 读状态\n"
        "- `GET /api/relays/{name}/raw?data=` — 原始帧\n"
        "- `GET /api/serial/ports` — 串口枚举\n"
        "- `GET /api/config/reload` — 重载配置\n");

    return pos;
}

void relay_http_page_docs(struct mg_connection *c, RelayHttpServer *srv,
                          struct mg_http_message *hm) {
    char *md;
    char base[192];
    char direct[128];
    size_t md_cap = 131072;
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
