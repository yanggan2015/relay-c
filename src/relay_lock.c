#include "relay_lock.h"

#include <pthread.h>

static pthread_mutex_t g_serial_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_action_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_inited = 0;

int relay_lock_init(void) {
    g_inited = 1;
    return 0;
}

void relay_lock_shutdown(void) {
    g_inited = 0;
}

void relay_serial_lock(void) {
    if (g_inited) pthread_mutex_lock(&g_serial_mtx);
}

void relay_serial_unlock(void) {
    if (g_inited) pthread_mutex_unlock(&g_serial_mtx);
}

int relay_action_try_lock(void) {
    if (!g_inited) return 1;
    return pthread_mutex_trylock(&g_action_mtx) == 0 ? 1 : 0;
}

void relay_action_unlock(void) {
    if (g_inited) pthread_mutex_unlock(&g_action_mtx);
}
