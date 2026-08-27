#ifndef RELAY_ACTION_H
#define RELAY_ACTION_H

#include "relay_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 根据 polarity_inverted 计算空闲态与脉冲态（逻辑电平，尚未经过 relay.io_inverted）：
 *   polarity_inverted=false（默认）: 空闲=ON(1), 脉冲=OFF(0)
 *     reset 固定时序: OFF -> 保持 hold_seconds -> ON
 *   polarity_inverted=true:          空闲=OFF(0), 脉冲=ON(1)
 *     reset 固定时序: ON -> 保持 hold_seconds -> OFF
 */
void relay_action_idle_pulse(int polarity_inverted, int *idle_out, int *pulse_out);

/* 逻辑电平 -> 经 relay.io_inverted 后的物理命令电平 */
int relay_logical_to_wire(const RelayConfig *relay, int logical);

/* 物理读回 -> 逻辑电平 */
int relay_wire_to_logical(const RelayConfig *relay, int wire);

const char *relay_action_mode_name(ActionSequenceMode mode);
ActionSequenceMode relay_action_mode_parse(const char *s);

#ifdef __cplusplus
}
#endif

#endif
