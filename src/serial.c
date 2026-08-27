#include "relay_serial.h"
#include "relay_protocol.h"
#include "relay_action.h"
#include "relay_lock.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

struct RelayExecutor {
    int dry_run;
};

static int g_dry_run = 0;

RelayExecutor *relay_executor_create(void) {
    RelayExecutor *ex = (RelayExecutor *)calloc(1, sizeof(RelayExecutor));
    if (ex) ex->dry_run = g_dry_run;
    return ex;
}

void relay_executor_free(RelayExecutor *ex) {
    free(ex);
}

void relay_executor_set_dry_run(int enabled) {
    g_dry_run = enabled ? 1 : 0;
}

#ifdef _WIN32
static HANDLE serial_open_win(const char *port, int baudrate) {
    char path[RELAY_MAX_PORT + 16];
    DCB dcb = {0};
    COMMTIMEOUTS timeouts = {0};
    HANDLE h;

    if (!port || !port[0]) return INVALID_HANDLE_VALUE;
    if (port[0] != '\\') {
        snprintf(path, sizeof(path), "\\\\.\\%s", port);
    } else {
        relay_str_copy(path, sizeof(path), port);
    }

    h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    dcb.BaudRate = (DWORD)baudrate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return h;
}

static int serial_write_win(HANDLE h, const uint8_t *data, size_t len) {
    DWORD written = 0;
    if (!WriteFile(h, data, (DWORD)len, &written, NULL)) return -1;
    return (written == len) ? 0 : -1;
}

static int serial_read_win(HANDLE h, uint8_t *buf, size_t cap, size_t *out_len) {
    DWORD n = 0;
    if (!ReadFile(h, buf, (DWORD)cap, &n, NULL)) return -1;
    if (out_len) *out_len = (size_t)n;
    return 0;
}

static void serial_close_win(HANDLE h) {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
#else
static speed_t baud_to_flag(int baud) {
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return B9600;
    }
}

static int serial_open_linux(const char *port, int baudrate) {
    struct termios tio;
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, baud_to_flag(baudrate));
    cfsetospeed(&tio, baud_to_flag(baudrate));
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    return fd;
}

static int serial_write_linux(int fd, const uint8_t *data, size_t len) {
    ssize_t n = write(fd, data, len);
    return (n == (ssize_t)len) ? 0 : -1;
}

static int serial_read_linux(int fd, uint8_t *buf, size_t cap, size_t *out_len) {
    ssize_t n = read(fd, buf, cap);
    if (n < 0) return -1;
    if (out_len) *out_len = (size_t)n;
    return 0;
}

static void serial_close_linux(int fd) {
    if (fd >= 0) close(fd);
}
#endif

static int port_transaction(const RelayConfig *relay, const char *port,
                            const RelayCommand *cmd, int channel,
                            int *state_out) {
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
    int baud = relay && relay->baudrate > 0 ? relay->baudrate : 9600;
    double delay = relay ? relay->post_write_delay_ms : 100.0;

    if (!port || !port[0] || !cmd) return -1;

    if (g_dry_run) {
        if (state_out) *state_out = 0;
        relay_sleep_ms((unsigned int)(delay > 0 ? delay : 10));
        return 0;
    }

    relay_serial_lock();
#ifdef _WIN32
    h = serial_open_win(port, baud);
    if (h == INVALID_HANDLE_VALUE) {
        relay_serial_unlock();
        return -1;
    }
#else
    fd = serial_open_linux(port, baud);
    if (fd < 0) {
        relay_serial_unlock();
        return -1;
    }
#endif

#ifdef _WIN32
    if (serial_write_win(h, cmd->bytes, cmd->len) != 0) {
        serial_close_win(h);
        relay_serial_unlock();
        return -1;
    }
#else
    if (serial_write_linux(fd, cmd->bytes, cmd->len) != 0) {
        serial_close_linux(fd);
        relay_serial_unlock();
        return -1;
    }
#endif

    relay_sleep_ms((unsigned int)(delay > 0 ? delay : 100));

    if (state_out && cmd->format == RELAY_FMT_A0 && cmd->has_response) {
        uint8_t resp[16];
        size_t rn = 0;
#ifdef _WIN32
        serial_read_win(h, resp, sizeof(resp), &rn);
#else
        serial_read_linux(fd, resp, sizeof(resp), &rn);
#endif
        if (relay_parse_a0_response(resp, rn, channel, state_out) != 0) {
            if (cmd->has_response && rn > (size_t)cmd->resp_offset) {
                *state_out = (resp[cmd->resp_offset] == cmd->resp_on_val) ? 1 : 0;
            } else {
                *state_out = -1;
            }
        }
    }

#ifdef _WIN32
    serial_close_win(h);
#else
    serial_close_linux(fd);
#endif
    relay_serial_unlock();
    return 0;
}

static void cache_set(RelayStateCache *cache, int relay_idx, int channel, int state) {
    if (!cache || relay_idx < 0 || relay_idx >= RELAY_MAX_RELAYS) return;
    if (channel < 1 || channel > RELAY_MAX_CHANNELS) return;
    cache->state[relay_idx][channel] = (int8_t)state;
}

int relay_executor_set_channel(RelayExecutor *ex, const RelayConfig *relay,
                               const char *port, int channel, int state,
                               RelayStateCache *cache, int relay_idx) {
    RelayCommand cmd;
    (void)ex;
    if (!relay || !relay->enabled) return -1;
    if (relay_resolve_command(relay, channel, state ? 1 : 0, 0, &cmd) != 0) return -1;
    if (cmd.format == RELAY_FMT_A0) {
        int wire = relay_logical_to_wire(relay, state ? 1 : 0);
        relay_build_a0_set(&cmd, channel, wire);
    } else if (relay->io_inverted) {
        RelayCommand alt;
        if (relay_resolve_command(relay, channel, state ? 0 : 1, 0, &alt) == 0 &&
            alt.format != RELAY_FMT_A0)
            cmd = alt;
    }
    if (port_transaction(relay, port, &cmd, channel, NULL) != 0) return -1;
    cache_set(cache, relay_idx, channel, state ? 1 : 0);
    return 0;
}

int relay_executor_get_channel(RelayExecutor *ex, const RelayConfig *relay,
                               const char *port, int channel, int *state_out,
                               RelayStateCache *cache, int relay_idx) {
    RelayCommand cmd;
    int st = -1;
    (void)ex;
    if (!relay || !relay->enabled || !state_out) return -1;
    if (relay_resolve_command(relay, channel, 0, 1, &cmd) != 0) return -1;
    if (port_transaction(relay, port, &cmd, channel, &st) != 0) {
        if (cache && relay_idx >= 0 && cache->state[relay_idx][channel] >= 0) {
            *state_out = cache->state[relay_idx][channel];
            return 0;
        }
        return -1;
    }
    if (st < 0) {
        if (cache && relay_idx >= 0 && cache->state[relay_idx][channel] >= 0) {
            *state_out = cache->state[relay_idx][channel];
            return 0;
        }
        *state_out = -1;
        return -1;
    }
    *state_out = relay_wire_to_logical(relay, st);
    cache_set(cache, relay_idx, channel, *state_out);
    return 0;
}

int relay_executor_send_raw(RelayExecutor *ex, const RelayConfig *relay,
                            const char *port, const uint8_t *data, size_t len) {
    RelayCommand cmd;
    (void)ex;
    if (!relay || !relay->enabled || !data || len == 0) return -1;
    memset(&cmd, 0, sizeof(cmd));
    cmd.format = RELAY_FMT_HEX;
    if (len > sizeof(cmd.bytes)) return -1;
    memcpy(cmd.bytes, data, len);
    cmd.len = len;
    return port_transaction(relay, port, &cmd, 0, NULL);
}

void relay_executor_sleep(double seconds) {
    if (seconds <= 0) return;
    if (g_dry_run) {
        relay_sleep_ms(10);
        return;
    }
    relay_sleep_ms((unsigned int)(seconds * 1000.0));
}
