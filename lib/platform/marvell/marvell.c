/*
 * (c) Copyright 2012 EVRYTHNG Ltd London / Zurich
 * www.evrythng.com
 */

#include "marvell/types.h"
#include "evrythng/platform.h"

#include <stdint.h>

#include <autoconf.h>
#include <wm_net.h>


void TimerInit(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }

	t->xTicksToWait = 0;
	memset(&t->xTimeOut, '\0', sizeof(t->xTimeOut));
}


void TimerDeinit(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }
}


char TimerIsExpired(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return -1;
    }

	return xTaskCheckForTimeOut(&t->xTimeOut, &t->xTicksToWait) == pdTRUE;
}


void TimerCountdownMS(Timer* t, unsigned int ms)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }

    t->xTicksToWait = os_msec_to_ticks(ms);
    vTaskSetTimeOutState(&t->xTimeOut); /* Record the time at which this function was entered. */
}


int TimerLeftMS(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return 0;
    }

    /* if true -> timeout, else updates xTicksToWait to the number left */
	if (xTaskCheckForTimeOut(&t->xTimeOut, &t->xTicksToWait) == pdTRUE)
        return 0;
	return (t->xTicksToWait <= 0) ? 0 : (os_ticks_to_msec(t->xTicksToWait));
}


void NetworkInit(Network* n)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return;
    }

    n->socket = 0;
    n->tls_enabled = 0;
}


void NetworkSecuredInit(Network* n, const char* ca_buf, size_t ca_size)
{
    if (!n || !ca_buf || !ca_size)
    {
        platform_printf("%s: bad args\n", __func__);
        return;
    }

	int rc = tls_lib_init();
	if (rc != WM_SUCCESS) 
    {
        platform_printf("%s: failed to init tls lib\n", __func__);
        return;
    }

	/* Initialize the TLS configuration structure */
    memset(&n->tls_config, 0, sizeof n->tls_config);
	n->tls_config = (tls_init_config_t){
		.flags = TLS_CHECK_SERVER_CERT,
		.tls.client.ca_cert = (unsigned char*)ca_buf,
		.tls.client.ca_cert_size = ca_size - 1,
	};

    n->tls_enabled = 1;
}


int NetworkConnect(Network* n, char* hostname, int port)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return -1;
    }

	int rc = -1;
    int attempt = 1;
	int type = SOCK_STREAM;
	struct sockaddr_in address;
	sa_family_t family = AF_INET;
	struct addrinfo *result = NULL;
	struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

    do
    {
        platform_printf("trying to resolve hostname (attempt %d)\n", attempt);

        if ((rc = getaddrinfo(hostname, NULL, &hints, &result)) == 0)
        {
            struct addrinfo* res = result;

            /* prefer ip4 addresses */
            while (res)
            {
                if (res->ai_family == AF_INET)
                {
                    result = res;
                    break;
                }
                res = res->ai_next;
            }

            if (result->ai_family == AF_INET)
            {
                address.sin_port = htons(port);
                address.sin_family = family = AF_INET;
                address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
            }
            else
                rc = -1;

            freeaddrinfo(result);
            break;
        }
        else
        {
            platform_printf("failed to resolve hostname %s, rc = %d\n", hostname, rc);
        }
    } 
    while (++attempt <= 5);

	if (rc == 0)
	{
		n->socket = socket(family, type, 0);
		if (n->socket != -1)
			rc = connect(n->socket, (struct sockaddr*)&address, sizeof(address));
	}

    if (rc != 0)
    {
        platform_printf("failed to connect to socket, rc = %d, errno = %d\n", rc, errno);
        return rc;
    }

    if (n->tls_enabled)
    {
        rc = tls_session_init(&n->tls_handle, n->socket, &n->tls_config);
        if (rc != WM_SUCCESS)
        {
            platform_printf("failed to init tls session, rc = %d\n", rc);
        }
    }

	return rc;
}


void NetworkDisconnect(Network* n)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return;
    }

    if (n->tls_enabled) tls_close(&n->tls_handle);
    shutdown(n->socket, SHUT_RDWR);
	close(n->socket);
}


int NetworkRead(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return -1;
    }

	int rc = setsockopt(n->socket, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout_ms, sizeof timeout_ms);
    if (rc != 0)
    {
        platform_printf("%s: failed to set socket option SO_RCVTIMEO, rc = %d\n", __func__, rc);
        return -1;
    }

	int bytes = 0;
	while (bytes < len)
	{
        if (!n->tls_enabled)
            rc = recv(n->socket, &buffer[bytes], (size_t)(len - bytes), 0);
        else
            rc = tls_recv(n->tls_handle, &buffer[bytes], (size_t)(len - bytes));

        if (rc == 0)
        {
            bytes = 0;
            break;
        }
        else if (rc == -1)
		{   
			if (errno != ENOTCONN && errno != ECONNRESET)
			{
				bytes = -1;
				break;
			}
		}
		else
			bytes += rc;
	}

    //platform_printf("%s: recv %d bytes\n", __func__, bytes);

	return bytes;
}


int NetworkWrite(Network* n, unsigned char* buffer, int length, int timeout_ms)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return -1;
    }

    int rc;
	setsockopt(n->socket, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout_ms, sizeof timeout_ms);

    if (!n->tls_enabled)
        rc = send(n->socket, buffer, length, 0);
    else
        rc = tls_send(n->tls_handle, buffer, length);

    //platform_printf("%s: send rc = %d\n", __func__, rc);

	return rc;
}


void MutexInit(Mutex* m)
{
    if (!m)
    {
        platform_printf("%s: invalid mutex %p\n", __func__, m);
        return;
    }

    if (os_mutex_create(&m->mutex, "mtx", OS_MUTEX_INHERIT) != WM_SUCCESS)
    {
        platform_printf("%s: failed to create mutex\n", __func__);
        return;
    }
}


int MutexLock(Mutex* m)
{
    if (!m)
    {
        platform_printf("%s: invalid mutex %p\n", __func__, m);
        return -1;
    }

    if (os_mutex_get(&m->mutex, OS_WAIT_FOREVER) != WM_SUCCESS)
    {
        platform_printf("%s: failed to lock mutex %p\n", __func__, m->mutex);
        return -1;
    }

    return 0;
}


int MutexUnlock(Mutex* m)
{
    if (!m)
    {
        platform_printf("%s: invalid mutex %p\n", __func__, m);
        return -1;
    }

    if (os_mutex_put(&m->mutex) != WM_SUCCESS)
    {
        platform_printf("%s: failed to UNlock mutex %p\n", __func__, m->mutex);
        return -1;
    }

    return 0;
}


void MutexDeinit(Mutex* m)
{
    if (!m)
    {
        platform_printf("%s: invalid mutex %p\n", __func__, m);
        return;
    }

    os_mutex_delete(&m->mutex);
}


void SemaphoreInit(Semaphore* s)
{
    if (!s)
    {
        platform_printf("%s: invalid semaphore %p\n", __func__, s);
        return;
    }

    if (os_semaphore_create_counting(&s->sem, "sem", 0xFFFF, 0) != WM_SUCCESS)
    {
        platform_printf("%s: failed to create semaphore\n", __func__);
        return;
    }
}


void SemaphoreDeinit(Semaphore* s)
{
    if (!s)
    {
        platform_printf("%s: invalid semaphore %p\n", __func__, s);
        return;
    }

    if (os_semaphore_delete(&s->sem) != WM_SUCCESS)
    {
        platform_printf("%s: failed to delete semaphore\n", __func__);
        return;
    }
}


int SemaphorePost(Semaphore* s)
{
    if (!s)
    {
        platform_printf("%s: invalid semaphore %p\n", __func__, s);
        return -1;
    }

    if (os_semaphore_put(&s->sem) != WM_SUCCESS)
    {
        platform_printf("%s: failed to post semaphore\n", __func__);
        return -1;
    }

    return 0;
}


int SemaphoreWait(Semaphore* s, int timeout_ms)
{
    if (!s)
    {
        platform_printf("%s: invalid semaphore %p\n", __func__, s);
        return -1;
    }

    if (os_semaphore_get(&s->sem, timeout_ms) != WM_SUCCESS)
    {
        return -1;
    }

    return 0;
}


static void func_wrapper(os_thread_arg_t arg)
{
    Thread* t = (Thread*)arg;
    (*t->func)(t->arg);

    SemaphorePost(&t->join_sem);

    os_thread_self_complete(0);
}


int ThreadCreate(Thread* t, 
        int priority, 
        const char* name, 
        void (*func)(void*), 
        size_t stack_size, 
        void* arg)
{
    if (!t) return -1;

    t->func = func;
    t->arg = arg;

    SemaphoreInit(&t->join_sem);

    os_thread_stack_define(thread_stack, stack_size);
    int rc = os_thread_create(&t->tid, name, func_wrapper, (os_thread_arg_t)t, &thread_stack, priority);
    if (rc != WM_SUCCESS)
    {
        SemaphoreDeinit(&t->join_sem);
        platform_printf("%s: failed to create thread: rc = %d\n", __func__, rc);
        return -1;
    }

    return 0;
}


int ThreadJoin(Thread* t, int timeout_ms)
{
    if (!t) return -1;

    if (SemaphoreWait(&t->join_sem, timeout_ms) != 0)
    {
        platform_printf("%s: timeout waiting for join\n", __func__);
        return -1;
    }

    return 0;
}


int ThreadDestroy(Thread* t)
{
    if (!t) return -1;

    if (os_thread_delete(&t->tid) != WM_SUCCESS)
    {
        platform_printf("%s: failed to delete thread\n", __func__);
    }

    SemaphoreDeinit(&t->join_sem);

    return 0;
}


void* platform_malloc(size_t bytes)
{   
    return os_mem_alloc(bytes);
}


void* platform_realloc(void* ptr, size_t bytes)
{
    return os_mem_realloc(ptr, bytes);
}


void platform_free(void* memory)
{
    os_mem_free(memory);
}


void platform_sleep(int ms)
{
    os_thread_sleep(os_msec_to_ticks(ms));
}


#if 1
int platform_printf(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);

    char msg[MAX_MSG_LEN];
    unsigned n = vsnprintf(msg, sizeof msg, fmt, vl);
    if (n >= sizeof msg)
        msg[sizeof msg - 1] = '\0';

    int rc = wmprintf("%s\r", msg);

    va_end(vl);

    wmstdio_flush();

    return rc;
}
#endif
