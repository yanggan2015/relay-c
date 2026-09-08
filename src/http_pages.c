#include "http_pages.h"
#include "relay_types.h"

#include <stdio.h>
#include <string.h>

const char *relay_page_styles(void) {
    return
        ":root{--bg:#eef3f6;--card:#fff;--ink:#132033;--muted:#5b6b82;--line:#d9e2ec;"
        "--brand:#0a6c74;--brand2:#0d8a94;--ok:#0f766e;--bad:#b42318;--warn:#bc5e00;"
        "--hero1:#0a6c74;--hero2:#0f3f48}"
        "html{color-scheme:light}"
        "*{box-sizing:border-box}"
        "body{margin:0;font-family:\"Segoe UI\",\"PingFang SC\",\"Microsoft YaHei\",sans-serif;"
        "background:radial-gradient(900px 420px at 8% -8%,#c8ecef 0%,transparent 55%),"
        "radial-gradient(700px 360px at 100% 0%,#dde7f8 0%,transparent 48%),var(--bg);color:var(--ink)}"
        ".wrap{max-width:1040px;margin:0 auto;padding:20px 16px 40px}"
        ".hero{background:linear-gradient(135deg,var(--hero1),var(--hero2));color:#fff;border-radius:18px;"
        "padding:22px 22px 18px;box-shadow:0 16px 36px rgba(10,108,116,.28);margin-bottom:14px}"
        ".hero-top{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;flex-wrap:wrap}"
        ".eyebrow{margin:0 0 6px;font-size:11px;font-weight:700;letter-spacing:.14em;text-transform:uppercase;opacity:.78}"
        ".hero h1{margin:0 0 6px;font-size:1.65rem;letter-spacing:.01em;font-weight:700}"
        ".hero p{margin:0;opacity:.92;line-height:1.5;font-size:14px;max-width:42em}"
        ".nav{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}"
        ".nav a{color:#fff;text-decoration:none;background:rgba(255,255,255,.14);padding:7px 13px;"
        "border-radius:10px;font-weight:700;font-size:13px}.nav a.active,.nav a:hover{background:rgba(255,255,255,.28)}"
        ".pill{display:inline-flex;align-items:center;gap:6px;padding:5px 11px;border-radius:999px;font-size:12px;font-weight:700;"
        "border:1px solid transparent}.pill.on{background:#ccfbf1;color:var(--ok)}.pill.off{background:#fee4e2;color:var(--bad)}"
        ".pill.hero-pill{background:rgba(255,255,255,.18);color:#fff;border-color:rgba(255,255,255,.28)}"
        ".pill.hero-pill.on{background:#ecfdf5;color:#065f46;border-color:transparent}"
        ".pill.hero-pill.off{background:rgba(254,226,226,.92);color:#991b1b;border-color:transparent}"
        ".kpi{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px;margin:0 0 14px}"
        "@media(max-width:860px){.kpi{grid-template-columns:repeat(2,minmax(0,1fr))}}"
        ".kpi .cell{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:10px 12px;"
        "box-shadow:0 4px 14px rgba(20,32,51,.04)}"
        ".kpi .k{font-size:11px;color:var(--muted);font-weight:700}.kpi .v{font-size:13px;font-weight:700;margin-top:3px;"
        "word-break:break-all;line-height:1.35}"
        ".grid{display:grid;grid-template-columns:1.08fr .92fr;gap:12px}"
        "@media(max-width:860px){.grid{grid-template-columns:1fr}}"
        ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;"
        "box-shadow:0 8px 22px rgba(20,32,51,.05)}"
        ".card.disabled{opacity:.55}"
        "h2{margin:0 0 10px;font-size:1.02rem}h3{margin:14px 0 8px;font-size:.92rem}"
        ".sub{color:var(--muted);font-size:13px;margin:0 0 12px;line-height:1.5}"
        ".row,.btn-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}"
        "label,.label{display:block;font-size:12px;font-weight:700;color:#334155;margin:0 0 4px}"
        ".field{margin-bottom:10px}"
        "input[type=text],input[type=number],input,select,textarea{width:100%;padding:9px 11px;"
        "border:1px solid var(--line);border-radius:10px;font:inherit;background:#fff;box-sizing:border-box}"
        "input:focus,select:focus,textarea:focus{outline:2px solid rgba(11,110,122,.25);border-color:var(--brand)}"
        "textarea{min-height:100px;font-family:Consolas,monospace;font-size:12px}"
        "button,.btn{border:0;border-radius:10px;padding:9px 13px;font:inherit;font-weight:700;cursor:pointer}"
        ".primary{background:var(--brand);color:#fff}.primary:hover{background:var(--brand2)}"
        ".ghost{background:#e8eef5;color:var(--ink)}.danger{background:#fee4e2;color:var(--bad)}"
        ".ok{background:#ccfbf1;color:var(--ok)}.warn{background:var(--warn);color:#fff}"
        "pre,code{font-family:Consolas,\"Cascadia Mono\",\"Sarasa Mono SC\",monospace}"
        "pre{background:#0f172a;color:#e2e8f0;border-radius:12px;padding:11px 12px;overflow:auto;font-size:12px;line-height:1.45;max-height:280px}"
        ".cmd{background:#f7fafc;border:1px solid var(--line);border-radius:12px;padding:11px;margin:0 0 10px}"
        ".cmd b{display:block;margin-bottom:6px}.cmd code{display:block;white-space:pre-wrap;color:var(--brand);font-size:12.5px}"
        ".list{margin:0;padding-left:18px}.list li{margin:4px 0;color:var(--muted)}"
        ".flash{display:none;margin-top:10px;padding:10px 12px;border-radius:10px;font-size:13px}"
        ".flash.show{display:block}.flash.ok{background:#ecfdf5;color:#065f46}.flash.err{background:#fef2f2;color:#991b1b}"
        ".foot{margin-top:14px;color:var(--muted);font-size:12px;text-align:center}"
        ".ch-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px;align-items:center}"
        ".ch-group{display:inline-flex;align-items:center;gap:4px;background:#f7fafc;border:1px solid var(--line);"
        "border-radius:10px;padding:4px 6px 4px 8px}"
        ".ch-label{font-size:12px;font-weight:700;color:var(--muted);min-width:2.2em}"
        ".ch-group button{padding:6px 10px;border-radius:8px;margin:0}"
        ".chip{display:inline-block;background:#f8fafc;border:1px solid var(--line);border-radius:8px;padding:6px 10px;margin:0 6px 6px 0;font-size:13px}"
        ".chip.clickable{cursor:pointer}.chip.active{border-color:var(--brand);background:#e6f7f5;box-shadow:0 0 0 1px var(--brand)}"
        ".chip:hover{background:#eef7f6}"
        ".status{margin-top:8px;padding:8px;border-radius:8px;font-size:13px;display:none}"
        ".status.ok{display:block;background:#e6f7f5;color:#124b46}"
        ".status.err{display:block;background:#fdecea;color:#7d231f}"
        ".meta-row{margin-bottom:8px}"
        ".config-layout{display:flex;flex-direction:column;gap:14px}"
        ".config-cols{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:start}"
        ".config-col{display:flex;flex-direction:column;gap:14px;min-width:0}"
        ".form-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}"
        ".doc-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px}"
        ".editor-hint{font-size:12px;color:var(--brand);font-weight:600;margin:0 0 10px}"
        ".warn-box{background:#fff7ed;border:1px solid #fdba74;color:#9a3412;border-radius:12px;padding:11px 12px;margin-bottom:12px;line-height:1.5}"
        "@media(max-width:900px){.config-cols{grid-template-columns:1fr}.form-grid{grid-template-columns:1fr 1fr}}"
        "@media(max-width:560px){.wrap{padding:12px 12px 28px}.hero{padding:16px}.form-grid{grid-template-columns:1fr}}";
}

const char *relay_page_head(const char *title) {
    static char buf[32768];
    snprintf(buf, sizeof(buf),
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"/>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
        "<title>%s</title><style>%s</style></head><body><div class=\"wrap\">",
        title ? title : "relay-c", relay_page_styles());
    return buf;
}

const char *relay_page_tail(void) {
    return "<p class=\"foot\">Embedded AI Development Kit</p></div></body></html>";
}

const char *relay_page_nav(const char *active) {
    static char nav[1024];
    snprintf(nav, sizeof(nav),
        "<header class=\"hero\">"
        "<div class=\"hero-top\"><div>"
        "<p class=\"eyebrow\">EADK · Tool</p>"
        "<h1>relay-c</h1>"
        "<p>继电器控制平台 v%s · 浏览器或桌面壳操作</p>"
        "</div></div>"
        "<nav class=\"nav\">"
        "<a href=\"/\" class=\"%s\">控制</a>"
        "<a href=\"/config\" class=\"%s\">配置</a>"
        "<a href=\"/docs\" class=\"%s\">说明</a>"
        "</nav></header>",
        RELAY_VERSION,
        (active && strcmp(active, "control") == 0) ? "active" : "",
        (active && strcmp(active, "config") == 0) ? "active" : "",
        (active && strcmp(active, "docs") == 0) ? "active" : "");
    return nav;
}
