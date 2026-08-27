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

static const char *DOCS_EXTRA_CSS =
    ".docs-tabs{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:14px}"
    ".docs-tab{border:1px solid var(--line);background:#fff;border-radius:999px;padding:8px 14px;"
    "cursor:pointer;font-weight:700;color:var(--ink,#13213a)}"
    ".docs-tab.active{background:var(--brand);color:#fff;border-color:var(--brand)}"
    ".docs-panel{display:none}.docs-panel.active{display:block}"
    ".markdown-body{line-height:1.6;font-size:14px}"
    ".markdown-body h1{font-size:1.6rem;margin:0 0 12px}"
    ".markdown-body h2{font-size:1.25rem;margin:18px 0 10px;border-bottom:1px solid var(--line);padding-bottom:6px}"
    ".markdown-body h3{font-size:1.05rem;margin:14px 0 8px}"
    ".markdown-body pre{background:#0f172a;color:#e5edf9;border-radius:10px;padding:12px;overflow:auto;font-size:13px}"
    ".markdown-body code{background:#eef2f7;padding:2px 6px;border-radius:4px;font-size:12px}"
    ".markdown-body pre code{background:transparent;padding:0;color:inherit}"
    ".markdown-body blockquote{border-left:4px solid var(--brand);margin:0 0 12px;padding:8px 12px;background:#e6f7f5;color:#124b46}"
    ".markdown-body ul{margin:8px 0 12px 20px}"
    "#mdSource{width:100%;min-height:320px;font-family:Consolas,monospace;font-size:12px;"
    "border:1px solid var(--line);border-radius:10px;padding:12px;box-sizing:border-box}";

static int host_is_local(const char *host) {
    if (!host || !host[0]) return 1;
    if (relay_str_eq_ci(host, "localhost")) return 1;
    if (strncmp(host, "127.", 4) == 0) return 1;
    if (strcmp(host, "::1") == 0) return 1;
    return 0;
}

static void docs_base_url(RelayHttpServer *srv, struct mg_http_message *hm,
                          char *base, size_t cap) {
    char host[256];
    char lan[64];
    char qhost[256];
    struct mg_str *hdr;
    int port = relay_http_port(srv);

    host[0] = qhost[0] = '\0';
    mg_http_get_var(&hm->query, "host", qhost, sizeof(qhost));
    if (qhost[0]) {
        relay_str_copy(host, sizeof(host), qhost);
    } else {
        hdr = mg_http_get_header(hm, "Host");
        if (hdr && hdr->len > 0)
            snprintf(host, sizeof(host), "%.*s", (int)hdr->len, hdr->buf);
    }

    if (host[0] && !host_is_local(host)) {
        if (strchr(host, ':'))
            snprintf(base, cap, "http://%s", host);
        else
            snprintf(base, cap, "http://%s:%d", host, port);
        return;
    }

    if (relay_detect_lan_ip(lan, sizeof(lan)) != 0 || !lan[0])
        relay_str_copy(lan, sizeof(lan), "127.0.0.1");
    snprintf(base, cap, "http://%s:%d", lan, port);
}

static size_t md_curl(char *md, size_t cap, size_t pos,
                      const char *label, const char *base, const char *path) {
    if (label && label[0])
        pos += (size_t)snprintf(md + pos, cap - pos, "\n**%s**\n\n", label);
    pos += (size_t)snprintf(md + pos, cap - pos,
                            "```bash\ncurl '%s%s'\n```\n", base, path);
    return pos;
}

static size_t docs_build_markdown(AppConfig *cfg, const char *base, char *md, size_t cap) {
    char path[512];
    char label[128];
    size_t pos = 0;
    int i, ch;

    pos += (size_t)snprintf(md + pos, cap - pos,
        "# Relay-C HTTP Command Docs\n\n"
        "> **AI 使用说明**：以下均为 **GET** 请求。请复制 `curl` 到局域网内其他主机执行（不要使用 localhost）。\n\n"
        "- Base URL: `%s`\n"
        "- Version: %s\n"
        "- Platform: %s\n\n",
        base, RELAY_VERSION, cfg->platform);

    if (cfg->n_boards > 0) {
        pos += (size_t)snprintf(md + pos, cap - pos,
            "## Board Commands\n\n");
        for (i = 0; i < cfg->n_boards; ++i) {
            const BoardConfig *b = &cfg->boards[i];
            pos += (size_t)snprintf(md + pos, cap - pos, "### board: `%s`\n\n", b->name);
            pos += (size_t)snprintf(md + pos, cap - pos,
                "- reset: `%s` ch%d, hold %.1fs\n",
                b->reset.relay_name, b->reset.channel, b->reset.hold_seconds);
            if (b->has_upgrade_mode && b->upgrade_mode.enabled)
                pos += (size_t)snprintf(md + pos, cap - pos,
                    "- upgrade: `%s` ch%d\n",
                    b->upgrade_mode.relay_name, b->upgrade_mode.channel);

            snprintf(path, sizeof(path), "/api/boards/%s/action?action=reset", b->name);
            pos = md_curl(md, cap, pos, "reset", base, path);
            if (b->has_upgrade_mode && b->upgrade_mode.enabled) {
                snprintf(path, sizeof(path), "/api/boards/%s/action?action=upgrade", b->name);
                pos = md_curl(md, cap, pos, "upgrade", base, path);
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
            pos = md_curl(md, cap, pos, "state", base, path);
        }
    }

    if (cfg->n_relays > 0) {
        pos += (size_t)snprintf(md + pos, cap - pos, "## Relay Commands\n\n");
        for (i = 0; i < cfg->n_relays; ++i) {
            const RelayConfig *r = &cfg->relays[i];
            pos += (size_t)snprintf(md + pos, cap - pos,
                "### relay: `%s` (%d channels)\n\n", r->name, r->relay_channels);
            for (ch = 1; ch <= r->relay_channels; ++ch) {
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
            snprintf(path, sizeof(path), "/api/relays/%s/state", r->name);
            pos = md_curl(md, cap, pos, "all state", base, path);
        }
    }

    pos = md_curl(md, cap, pos, "health", base, "/api/health");
    return pos;
}

void relay_http_page_docs(struct mg_connection *c, RelayHttpServer *srv,
                          struct mg_http_message *hm) {
    char *md, *html;
    char base[128];
    char fmt[16];
    size_t md_cap = 131072;
    size_t html_cap = 262144;
    size_t md_len, pos;
    AppConfig *cfg = relay_http_config(srv);

    fmt[0] = '\0';
    mg_http_get_var(&hm->query, "format", fmt, sizeof(fmt));

    md = (char *)malloc(md_cap);
    if (!md || !cfg) {
        mg_http_reply(c, 500, "", "oom");
        free(md);
        return;
    }

    docs_base_url(srv, hm, base, sizeof(base));
    md_len = docs_build_markdown(cfg, base, md, md_cap);

    if (relay_str_eq_ci(fmt, "md") || relay_str_eq_ci(fmt, "markdown")) {
        mg_http_reply(c, 200,
                      "Content-Type: text/markdown; charset=utf-8\r\nCache-Control: no-store\r\n",
                      "%s", md);
        free(md);
        return;
    }

    html = (char *)malloc(html_cap);
    if (!html) {
        mg_http_reply(c, 500, "", "oom");
        free(md);
        return;
    }

    pos = (size_t)snprintf(html, html_cap, "%s%s<style>%s</style>",
                           relay_page_head("Relay-C HTTP Docs"), relay_page_nav("docs"),
                           DOCS_EXTRA_CSS);
    pos += (size_t)snprintf(html + pos, html_cap - pos,
        "<article class=\"card\">"
        "<h2>GET Command Docs</h2>"
        "<p class=\"sub\">Base URL: <strong>%s</strong> · "
        "<a href=\"/docs?format=md\">纯 Markdown</a></p>"
        "<div class=\"docs-tabs\">"
        "<button type=\"button\" class=\"docs-tab active\" onclick=\"showDocsTab('preview')\">Markdown 预览</button>"
        "<button type=\"button\" class=\"docs-tab\" onclick=\"showDocsTab('source')\">Markdown 源码</button>"
        "</div>"
        "<div id=\"panel-preview\" class=\"docs-panel active\">"
        "<div id=\"mdPreview\" class=\"markdown-body\"></div>"
        "</div>"
        "<div id=\"panel-source\" class=\"docs-panel\">"
        "<div class=\"btn-row\">"
        "<button type=\"button\" class=\"primary\" onclick=\"copyAllMd()\">复制全部 Markdown</button>"
        "</div>"
        "<textarea id=\"mdSource\" readonly></textarea>"
        "</div></article>",
        base);

    pos += (size_t)snprintf(html + pos, html_cap - pos,
        "<script type=\"text/plain\" id=\"mdData\">");
    if (pos + md_len + 64 < html_cap)
        memcpy(html + pos, md, md_len), pos += md_len;
    pos += (size_t)snprintf(html + pos, html_cap - pos,
        "</script>"
        "<script src=\"https://cdn.jsdelivr.net/npm/marked/marked.min.js\"></script>"
        "<script>"
        "const mdEl=document.getElementById('mdData');"
        "const md=mdEl?mdEl.textContent:'';"
        "document.getElementById('mdSource').value=md;"
        "function renderMd(){"
        "  if(window.marked&&md){document.getElementById('mdPreview').innerHTML=marked.parse(md);}"
        "  else{document.getElementById('mdPreview').innerHTML='<pre></pre>';"
        "    document.getElementById('mdPreview').firstChild.textContent=md;}"
        "}"
        "function showDocsTab(name){"
        "  const names=['preview','source'];"
        "  document.querySelectorAll('.docs-tab').forEach((t,i)=>t.classList.toggle('active',names[i]===name));"
        "  names.forEach(n=>document.getElementById('panel-'+n).classList.toggle('active',n===name));"
        "}"
        "function copyAllMd(){navigator.clipboard.writeText(md);alert('Markdown 已复制');}"
        "renderMd();"
        "</script>%s", relay_page_tail());

    mg_http_reply(c, 200,
                  "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s", html);
    free(md);
    free(html);
}
