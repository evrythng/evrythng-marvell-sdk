/*
 * (c) Copyright 2012 EVRYTHNG Ltd London / Zurich
 * www.evrythng.com
 */

#if !defined(_MQTT_MARVELL_)
#define _MQTT_MARVELL_

#include <stddef.h>
#include <wm_os.h>
#include <wm-tls.h>

#include "FreeRTOS.h"
#include "task.h"

typedef struct Timer
{
	portTickType xTicksToWait;
	xTimeOutType xTimeOut;
} Timer;

typedef struct Network
{
    int socket;
    int tls_enabled;
    tls_handle_t tls_handle;
    tls_init_config_t tls_config;
} Network;

typedef struct Mutex
{
    os_mutex_t mutex;
} Mutex;

typedef struct Semaphore
{
    os_semaphore_t sem;
} Semaphore;

typedef struct Thread
{
    os_thread_t tid;
    void* arg;
    void (*func)(void*);
    Semaphore join_sem;
} Thread;

#endif //_MQTT_MARVELL_
