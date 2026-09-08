/*
 * Windows desktop shell: WebView2 + relay-c.exe.
 * Shared webview headers: ../../ssh-bridge-c/third_party
 */
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "webview.h"
#include "desktop_res.h"

namespace {

constexpr int kDefaultHttpPort = 18053;
constexpr int kHealthTimeoutMs = 30000;
constexpr int kHealthIntervalMs = 200;
constexpr const wchar_t *kChildExe = L"relay-c.exe";
constexpr const wchar_t *kDefaultConfig = L"boards.json";
constexpr const char *kAppTitle = "relay-c";

struct Options {
  std::wstring config_path;
  int http_port = -1;
  bool smoke_test = false;
  bool show_help = false;
};

std::wstring exe_dir() {
  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return L".";
  for (DWORD i = n; i > 0; --i) {
    if (path[i - 1] == L'\\' || path[i - 1] == L'/') {
      path[i - 1] = L'\0';
      break;
    }
  }
  return path;
}

std::wstring join_path(const std::wstring &dir, const wchar_t *name) {
  if (dir.empty()) return name;
  if (dir.back() == L'\\' || dir.back() == L'/') return dir + name;
  return dir + L"\\" + name;
}

std::wstring widen(const std::string &s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  if (n <= 0) return {};
  std::wstring out((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
  return out;
}

void show_msg(const wchar_t *text) {
  MessageBoxW(nullptr, text, widen(kAppTitle).c_str(), MB_OK | MB_ICONERROR);
}

bool file_exists(const std::wstring &path) {
  return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring resolve_config_path(const Options &o, const std::wstring &dir) {
  if (!o.config_path.empty()) return o.config_path;
  std::wstring boards = join_path(dir, kDefaultConfig);
  if (file_exists(boards)) return boards;
  std::wstring example = join_path(dir, L"boards.json.example");
  if (file_exists(example) && CopyFileW(example.c_str(), boards.c_str(), FALSE)) return boards;
  return boards;
}

Options parse_args(int argc, wchar_t **argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    std::wstring a = argv[i] ? argv[i] : L"";
    if (a == L"-h" || a == L"--help" || a == L"/?") {
      o.show_help = true;
    } else if (a == L"--smoke-test") {
      o.smoke_test = true;
    } else if ((a == L"-c" || a == L"--config") && i + 1 < argc) {
      o.config_path = argv[++i];
    } else if (a == L"--http-port" && i + 1 < argc) {
      o.http_port = _wtoi(argv[++i]);
    }
  }
  return o;
}

/* Prefer "http_port", else "port" (relay server.port). Best-effort, no JSON lib. */
int read_http_port_from_config(const std::wstring &path, int fallback) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return fallback;
  LARGE_INTEGER sz{};
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024) {
    CloseHandle(h);
    return fallback;
  }
  std::string data((size_t)sz.QuadPart, '\0');
  DWORD rd = 0;
  if (!ReadFile(h, &data[0], (DWORD)sz.QuadPart, &rd, nullptr)) {
    CloseHandle(h);
    return fallback;
  }
  CloseHandle(h);
  data.resize(rd);

  size_t pos = data.find("\"http_port\"");
  if (pos == std::string::npos) pos = data.find("\"port\"");
  if (pos == std::string::npos) return fallback;
  pos = data.find(':', pos);
  if (pos == std::string::npos) return fallback;
  ++pos;
  while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t')) ++pos;
  int port = 0;
  if (pos >= data.size() || data[pos] < '0' || data[pos] > '9') return fallback;
  while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
    port = port * 10 + (data[pos] - '0');
    ++pos;
  }
  if (port < 1 || port > 65535) return fallback;
  return port;
}

int resolve_http_port(const Options &o, const std::wstring &dir) {
  if (o.http_port > 0) return o.http_port;
  std::wstring cfg = resolve_config_path(o, dir);
  if (!cfg.empty() && file_exists(cfg)) return read_http_port_from_config(cfg, kDefaultHttpPort);
  return kDefaultHttpPort;
}

bool tcp_get_ok(const char *host, int port, const char *path) {
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) {
    WSACleanup();
    return false;
  }
  DWORD timeout = 800;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);
  inet_pton(AF_INET, host, &addr.sin_addr);

  bool ok = false;
  if (connect(s, (sockaddr *)&addr, sizeof(addr)) == 0) {
    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", path, host, port);
    if (send(s, req, (int)strlen(req), 0) > 0) {
      char buf[256];
      int n = recv(s, buf, sizeof(buf) - 1, 0);
      if (n > 0) {
        buf[n] = '\0';
        ok = (strstr(buf, "200") != nullptr);
      }
    }
  }
  closesocket(s);
  WSACleanup();
  return ok;
}

bool wait_health(int port, int timeout_ms) {
  int waited = 0;
  while (waited < timeout_ms) {
    if (tcp_get_ok("127.0.0.1", port, "/health")) return true;
    Sleep(kHealthIntervalMs);
    waited += kHealthIntervalMs;
  }
  return false;
}

struct ChildProc {
  PROCESS_INFORMATION pi{};
  HANDLE job = nullptr;
  bool ok = false;
};

void close_child(ChildProc &c) {
  if (c.pi.hProcess) {
    if (WaitForSingleObject(c.pi.hProcess, 0) == WAIT_TIMEOUT) {
      TerminateProcess(c.pi.hProcess, 1);
      WaitForSingleObject(c.pi.hProcess, 3000);
    }
    CloseHandle(c.pi.hProcess);
    c.pi.hProcess = nullptr;
  }
  if (c.pi.hThread) {
    CloseHandle(c.pi.hThread);
    c.pi.hThread = nullptr;
  }
  if (c.job) {
    CloseHandle(c.job);
    c.job = nullptr;
  }
  c.ok = false;
}

bool start_child(ChildProc &out, const std::wstring &dir, const Options &o, int http_port) {
  std::wstring exe = join_path(dir, kChildExe);
  if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
    show_msg(L"未找到同目录下的 relay-c.exe");
    return false;
  }

  std::wstring cfg = o.config_path;
  if (cfg.empty()) cfg = resolve_config_path(o, dir);

  std::wstring cmd = L"\"" + exe + L"\" -c \"" + cfg + L"\"";
  if (o.http_port > 0) {
    cmd += L" -p ";
    cmd += std::to_wstring(http_port);
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back(L'\0');

  BOOL created = CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                                CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, dir.c_str(), &si,
                                &pi);
  if (!created) {
    show_msg(L"无法启动 relay-c.exe");
    return false;
  }

  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    if (!AssignProcessToJobObject(job, pi.hProcess)) {
      CloseHandle(job);
      job = nullptr;
    }
  }

  ResumeThread(pi.hThread);

  out.pi = pi;
  out.job = job;
  out.ok = true;
  return true;
}

bool child_alive(const ChildProc &c) {
  if (!c.pi.hProcess) return false;
  return WaitForSingleObject(c.pi.hProcess, 0) == WAIT_TIMEOUT;
}

void print_help_console() {
  AllocConsole();
  FILE *fp = nullptr;
  freopen_s(&fp, "CONOUT$", "w", stdout);
  freopen_s(&fp, "CONOUT$", "w", stderr);
  printf("%s (desktop)\n", kAppTitle);
  printf("Usage: relay_desktop.exe [options]\n");
  printf("  -c, --config PATH   boards.json path (passed to relay-c)\n");
  printf("  --http-port N       HTTP port (default from config or %d)\n", kDefaultHttpPort);
  printf("  --smoke-test        start child, wait /health, exit (no UI)\n");
  printf("  -h, --help          show this help\n");
  printf("\nStarts relay-c.exe in the same folder and embeds the WebUI.\n");
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  Options opt = parse_args(argc, argv ? argv : nullptr);
  if (argv) LocalFree(argv);

  if (opt.show_help) {
    print_help_console();
    return 0;
  }

  std::wstring dir = exe_dir();
  if (opt.config_path.empty()) opt.config_path = resolve_config_path(opt, dir);
  int port = resolve_http_port(opt, dir);

  ChildProc child;
  if (!start_child(child, dir, opt, port)) return 1;

  if (!wait_health(port, kHealthTimeoutMs)) {
    if (!child_alive(child)) {
      show_msg(L"后端进程已退出，HTTP 未能就绪。请检查配置或端口占用。");
    } else {
      show_msg(L"等待 HTTP 服务超时。请确认端口未被占用，或安装 WebView2 Runtime。");
    }
    close_child(child);
    return 2;
  }

  if (opt.smoke_test) {
    close_child(child);
    return 0;
  }

  std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
  webview::webview w(false, nullptr);
  if (!w.window()) {
    show_msg(L"无法创建 WebView2 窗口。\n\n请安装 Microsoft Edge WebView2 Runtime:\n"
             L"https://developer.microsoft.com/microsoft-edge/webview2/");
    close_child(child);
    return 3;
  }
  w.set_title(kAppTitle);
  w.set_size(900, 640, WEBVIEW_HINT_MIN);
  w.set_size(1080, 740, WEBVIEW_HINT_NONE);
  {
    HWND hwnd = (HWND)w.window();
    HICON hi = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON));
    if (hwnd && hi) {
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hi);
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hi);
    }
  }
  w.navigate(url);
  w.run();

  close_child(child);
  return 0;
}
