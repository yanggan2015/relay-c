#ifndef RELAY_PORTS_H
#define RELAY_PORTS_H

#include <cjson/cJSON.h>

/* 枚举本机可用串口，返回 JSON：platform / windows_ports / linux_ports */
cJSON *relay_ports_enumerate_json(void);

#endif
