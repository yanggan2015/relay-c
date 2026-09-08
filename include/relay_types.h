#ifndef RELAY_TYPES_H
#define RELAY_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RELAY_MAX_NAME           64
#define RELAY_MAX_LABEL          64
#define RELAY_MAX_PORT           128
#define RELAY_MAX_CHANNELS       16
#define RELAY_MAX_CMD_BYTES      256
#define RELAY_MAX_RELAYS         32
#define RELAY_MAX_BOARDS         32
#define RELAY_MAX_CUSTOM_ACTIONS 8
#define RELAY_MAX_PATH           512

#if defined(__has_include)
#  if __has_include("version_gen.h")
#    include "version_gen.h"
#  else
#    include "version_fallback.h"
#  endif
#else
#  include "version_fallback.h"
#endif

#define RELAY_VERSION            APP_VERSION_STRING

/* relay 启动时通道初始化：-1=不操作，0=全 OFF，1=全 ON */
#define RELAY_STARTUP_SKIP       (-1)

typedef enum {
    RELAY_FMT_A0 = 0,
    RELAY_FMT_HEX,
    RELAY_FMT_STR,
    RELAY_FMT_NONE
} RelayCmdFormat;

/* 自定义动作开关逻辑 */
typedef enum {
    ACTION_MODE_PULSE = 0,   /* 空闲态 -> 脉冲态 -> 保持 -> 回空闲态（同 reset） */
    ACTION_MODE_HOLD_ON,     /* 置 ON 并保持 */
    ACTION_MODE_HOLD_OFF     /* 置 OFF 并保持 */
} ActionSequenceMode;

typedef struct {
    RelayCmdFormat format;
    uint8_t        bytes[RELAY_MAX_CMD_BYTES];
    size_t         len;
    int            a0_on;
    int            has_response;
    int            resp_offset;
    uint8_t        resp_on_val;
} RelayCommand;

typedef struct {
    RelayCommand on;
    RelayCommand off;
    RelayCommand query;
    int          custom;
} ChannelCommands;

typedef struct {
    char             name[RELAY_MAX_NAME];
    int              enabled;
    char             port[RELAY_MAX_PORT];
    char             linux_port[RELAY_MAX_PORT];
    char             windows_port[RELAY_MAX_PORT];
    int              relay_channels;
    int              baudrate;
    int              io_inverted; /* 继电器 IO 接线取反：逻辑 ON/OFF 与物理命令对调 */
    int              startup_init; /* RELAY_STARTUP_SKIP / 0 / 1 */
    double           post_write_delay_ms;
    ChannelCommands  channels[RELAY_MAX_CHANNELS + 1];
} RelayConfig;

typedef struct {
    int              enabled;
    char             relay_name[RELAY_MAX_NAME];
    int              channel;
    int              polarity_inverted; /* false: 空闲=ON 脉冲=OFF；true: 空闲=OFF 脉冲=ON */
    double           hold_seconds;
} BoardActionBinding;

typedef struct {
    char                 name[RELAY_MAX_NAME];  /* API 动作名，如 power_cycle */
    char                 label[RELAY_MAX_LABEL]; /* 界面显示标签 */
    int                  enabled;
    char                 relay_name[RELAY_MAX_NAME];
    int                  channel;
    int                  polarity_inverted;
    double               hold_seconds;
    ActionSequenceMode   mode;
} BoardCustomAction;

typedef struct {
    char                 name[RELAY_MAX_NAME];
    int                  enabled;
    BoardActionBinding   reset;
    int                  has_upgrade_mode;
    BoardActionBinding   upgrade_mode;
    BoardCustomAction    custom_actions[RELAY_MAX_CUSTOM_ACTIONS];
    int                  n_custom_actions;
} BoardConfig;

typedef struct {
    int          server_port;
    int          server_port_set;
    int          discoverable; /* default 1 */
    char         display_name[128];
    RelayConfig  relays[RELAY_MAX_RELAYS];
    int          n_relays;
    BoardConfig  boards[RELAY_MAX_BOARDS];
    int          n_boards;
    char         config_path[RELAY_MAX_PATH];
    char         platform[16];
} AppConfig;

typedef struct {
    int8_t state[RELAY_MAX_RELAYS][RELAY_MAX_CHANNELS + 1];
} RelayStateCache;

#ifdef __cplusplus
}
#endif

#endif
