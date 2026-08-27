#include "http_pages.h"
#include "relay_types.h"

#include <stdio.h>
#include <string.h>

const char *relay_page_styles(void) {
    return
        ":root{--brand:#0a6c74;--warn:#bc5e00;--line:#ddd;--muted:#46506a}"
        "body{font-family:Segoe UI,sans-serif;margin:0;background:#f0f4f8;color:#13213a}"
        ".wrap{max-width:1200px;margin:0 auto;padding:20px}"
        ".hero{background:#fff;border-radius:16px;padding:20px;margin-bottom:16px;box-shadow:0 4px 20px rgba(0,0,0,.08)}"
        "h1{margin:0 0 8px}h2{margin:0 0 12px;font-size:1.25rem}h3{margin:0 0 10px;font-size:1.05rem}"
        ".nav a{margin-right:12px;color:var(--brand);font-weight:700;text-decoration:none;padding:6px 10px;border-radius:8px}"
        ".nav a.active,.nav a:hover{background:#e6f7f5}"
        ".config-layout{display:flex;flex-direction:column;gap:14px}"
        ".config-cols{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:start}"
        ".config-col{display:flex;flex-direction:column;gap:14px;min-width:0}"
        ".card{background:#fff;border-radius:12px;padding:16px;box-shadow:0 2px 10px rgba(0,0,0,.06)}"
        ".card.disabled{opacity:.55}"
        ".form-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}"
        ".doc-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px}"
        ".label{display:block;font-size:12px;font-weight:700;margin-bottom:4px;color:#344054}"
        "input,select,textarea{width:100%;box-sizing:border-box;border:1px solid var(--line);border-radius:8px;padding:8px;font:inherit}"
        "textarea{min-height:100px;font-family:Consolas,monospace;font-size:12px}"
        "pre{background:#f8fafc;border:1px solid var(--line);border-radius:8px;padding:10px;margin:6px 0;overflow-x:auto;font-size:12px}"
        "button{border:0;border-radius:8px;padding:8px 12px;margin:0;cursor:pointer;font-weight:600;font:inherit}"
        ".btn-row,.ch-row{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}"
        ".primary{background:var(--brand);color:#fff}.warn{background:var(--warn);color:#fff}"
        ".ghost{background:#eef2f7;color:#13213a}.danger{background:#bd1e1e;color:#fff}"
        ".chip{display:inline-block;background:#f8fafc;border:1px solid var(--line);border-radius:8px;padding:6px 10px;margin:0 6px 6px 0;font-size:13px}"
        ".chip.clickable{cursor:pointer}.chip.active{border-color:var(--brand);background:#e6f7f5;box-shadow:0 0 0 1px var(--brand)}"
        ".chip:hover{background:#eef7f6}"
        ".editor-hint{font-size:12px;color:var(--brand);font-weight:600;margin:0 0 10px}"
        ".status{margin-top:8px;padding:8px;border-radius:8px;font-size:13px;display:none}"
        ".status.ok{display:block;background:#e6f7f5;color:#124b46}"
        ".status.err{display:block;background:#fdecea;color:#7d231f}"
        ".sub{color:var(--muted);font-size:13px;margin:0 0 12px}"
        ".meta-row{margin-bottom:8px}"
        "@media(max-width:900px){.config-cols{grid-template-columns:1fr}.form-grid{grid-template-columns:1fr 1fr}}"
        "@media(max-width:560px){.wrap{padding:12px}.hero{padding:14px}.form-grid{grid-template-columns:1fr}}";
}

const char *relay_page_head(const char *title) {
    static char buf[8192];
    snprintf(buf, sizeof(buf),
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"/>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
        "<title>%s</title><style>%s</style></head><body><div class=\"wrap\">",
        title ? title : "Relay-C", relay_page_styles());
    return buf;
}

const char *relay_page_tail(void) {
    return "</div></body></html>";
}

const char *relay_page_nav(const char *active) {
    static char nav[512];
    snprintf(nav, sizeof(nav),
        "<header class=\"hero\"><h1>Relay-C</h1>"
        "<p>继电器控制平台 v%s</p>"
        "<nav class=\"nav\">"
        "<a href=\"/\" class=\"%s\">Control</a>"
        "<a href=\"/config\" class=\"%s\">Config</a>"
        "<a href=\"/docs\" class=\"%s\">Docs</a>"
        "<a href=\"/api/health\" class=\"%s\">Health</a>"
        "</nav></header>",
        RELAY_VERSION,
        (active && strcmp(active, "control") == 0) ? "active" : "",
        (active && strcmp(active, "config") == 0) ? "active" : "",
        (active && strcmp(active, "docs") == 0) ? "active" : "",
        (active && strcmp(active, "health") == 0) ? "active" : "");
    return nav;
}
