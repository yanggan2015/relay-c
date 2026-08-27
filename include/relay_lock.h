#ifndef RELAY_LOCK_H
#define RELAY_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

int  relay_lock_init(void);
void relay_lock_shutdown(void);

/* 串口读写全局互斥：同一时刻只允许一个串口事务 */
void relay_serial_lock(void);
void relay_serial_unlock(void);

/* 板级动作互斥：reset/upgrade/custom 不可并发，避免时序冲突 */
int  relay_action_try_lock(void); /* 1= acquired, 0= busy */
void relay_action_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
