/*
 * (c) Copyright 2012 EVRYTHNG Ltd London / Zurich
 * www.evrythng.com
 */

#include "evrythng/platform.h"

#include <stdint.h>
#include <stdarg.h>

#include <wm_net.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>


void platform_timer_init(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }

	t->xTicksToWait = 0;
	memset(&t->xTimeOut, '\0', sizeof(t->xTimeOut));
}


void platform_timer_deinit(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }
}


char platform_timer_isexpired(Timer* t)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return -1;
    }

	return xTaskCheckForTimeOut(&t->xTimeOut, &t->xTicksToWait) == pdTRUE;
}


void platform_timer_countdown(Timer* t, unsigned int ms)
{
    if (!t)
    {
        platform_printf("%s: invalid timer\n", __func__);
        return;
    }

    t->xTicksToWait = os_msec_to_ticks(ms);
    vTaskSetTimeOutState(&t->xTimeOut); /* Record the time at which this function was entered. */
}


int platform_timer_left(Timer* t)
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


void platform_network_init(Network* n)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return;
    }

    memset(n, 0, sizeof(Network));
}


void platform_network_securedinit(Network* n, const char* ca_buf, size_t ca_size)
{
    if (!n || !ca_buf || !ca_size)
    {
        platform_printf("%s: bad args\n", __func__);
        return;
    }

    memset(n, 0, sizeof(Network));

    n->ca_buf = ca_buf;
    n->ca_size = ca_size;

    n->tls_enabled = 1;
}

static const mbedtls_x509_crt_profile wm_mbedtls_x509_crt_profile_evrythng = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD2)     |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD4)     |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD5)     |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1)    |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224)  |
	MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256)  |
	MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384)  |
	MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512)  |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_RIPEMD160),
	0xFFFFFFF, /* Any PK alg    */
	0xFFFFFFF, /* Any curve     */
	1024,
};

static int tls_connect(Network* n, const char* hostname)
{
    int rc = -1;

    n->tls_cert.ca_chain = wm_mbedtls_parse_cert(
            (const unsigned char*)n->ca_buf, n->ca_size);
    if (!n->tls_cert.ca_chain) {
        platform_printf("%s: failed to parse certificate chain\n", __func__);
        return rc;
    }

    n->tls_config = wm_mbedtls_ssl_config_new(&n->tls_cert, 
            MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_VERIFY_REQUIRED);
    if (!n->tls_config) {
        platform_printf("%s: failed to create tls config\n", __func__);
        return rc;
    }

    mbedtls_ssl_conf_min_version(n->tls_config, 
            MBEDTLS_SSL_MAJOR_VERSION_3,
            MBEDTLS_SSL_MINOR_VERSION_3);

	mbedtls_ssl_conf_cert_profile(n->tls_config,
			&wm_mbedtls_x509_crt_profile_evrythng);

    n->tls_context = wm_mbedtls_ssl_new(n->tls_config, 
            n->socket, (const char*) hostname);
    if (!n->tls_context) {
        platform_printf("Failed to create tls context\n");
        return rc;
    }

    rc = wm_mbedtls_ssl_connect(n->tls_context);
    if (rc != 0) {
        platform_printf("tls connection failed: 0x%02x\n", rc);
        return rc;
    }

    return 0;
}


static int tcp_connect(Network* n, const char* hostname, int port)
{
	int rc = -1;
    int attempt = 1;
	int type = SOCK_STREAM;
	struct sockaddr_in address;
	sa_family_t family = AF_INET;
	struct addrinfo *result = NULL;
	struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

    do {
        platform_printf("trying to resolve hostname (attempt %d)\n", attempt);

        if ((rc = getaddrinfo(hostname, NULL, &hints, &result)) == 0) {
            struct addrinfo* res = result;

            /* prefer ip4 addresses */
            while (res) {
                if (res->ai_family == AF_INET) {
                    result = res;
                    break;
                }
                res = res->ai_next;
            }

            if (result->ai_family == AF_INET) {
                address.sin_port = htons(port);
                address.sin_family = family = AF_INET;
                address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
            }
            else
                rc = -1;

            freeaddrinfo(result);
            break;
        } else {
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

    if (rc != 0) {
        platform_printf("failed to connect to socket, rc = %d, errno = %d\n", rc, errno);
    }

    return rc;
}


int platform_network_connect(Network* n, char* hostname, int port)
{
    int rc;

    if (!n) {
        platform_printf("%s: invalid network\n", __func__);
        return -1;
    }

    rc = tcp_connect(n, hostname, port); 

    if (!rc && n->tls_enabled) {
        rc = tls_connect(n, hostname);
    }

	return rc;
}


void platform_network_disconnect(Network* n)
{
    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return;
    }

    if (n->tls_enabled) {
		if (n->tls_context) {
            mbedtls_ssl_close_notify(n->tls_context);
            wm_mbedtls_ssl_free(n->tls_context);
        }
        if (n->tls_config) wm_mbedtls_ssl_config_free(n->tls_config);
        if (n->tls_cert.ca_chain) wm_mbedtls_free_cert(n->tls_cert.ca_chain);

        n->tls_context = 0;
        n->tls_config = 0;
        n->tls_cert.ca_chain = 0;
    }

    shutdown(n->socket, SHUT_RDWR);
	close(n->socket);
}


int platform_network_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    int rc;

    if (!n)
    {
        platform_printf("%s: invalid network\n", __func__);
        return -1;
    }

    if (!n->tls_enabled) {
        rc = setsockopt(n->socket, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout_ms, sizeof timeout_ms);
        if (rc != 0) {
            platform_printf("%s: failed to set socket option SO_RCVTIMEO, rc = %d\n", __func__, rc);
            return -1;
        }
    } else {
        wm_mbedtls_reset_read_timer(n->tls_context);
        wm_mbedtls_set_read_timeout(n->tls_context, timeout_ms);
    }

	int bytes = 0;
	while (bytes < len)
	{
        if (!n->tls_enabled)
            rc = recv(n->socket, &buffer[bytes], (size_t)(len - bytes), 0);
        else {
            rc = mbedtls_ssl_read(n->tls_context, &buffer[bytes], (size_t)(len - bytes));
        }

        if (rc == 0)
        {
            bytes = 0;
            break;
        }
        else if (rc < 0)
        {   
            if (n->tls_enabled)
            {
                if (rc == MBEDTLS_ERR_SSL_TIMEOUT ||
                        rc == MBEDTLS_ERR_NET_RECV_FAILED)
                {
                    bytes = -1;
                    break;
                }
                else
                    platform_printf("mbedtls_ssl_read ret: -0x%02X\n", -rc);
            }
            else
            {
                if (errno != ENOTCONN && errno != ECONNRESET)
                {
                    bytes = -1;
                    break;
                }
            }
        }
        else
            bytes += rc;
    }

    if (!bytes)
        platform_printf("%s:%d: connection closed by the peer\n", 
                __func__, __LINE__);

	return bytes;
}


int platform_network_write(Network* n, unsigned char* buffer, int length, int timeout_ms)
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
		rc = mbedtls_ssl_write(n->tls_context, buffer, length);

    //platform_printf("%s: send rc = %d\n", __func__, rc);

	return rc;
}


void platform_mutex_init(Mutex* m)
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


int platform_mutex_lock(Mutex* m)
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


int platform_mutex_unlock(Mutex* m)
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


void platform_mutex_deinit(Mutex* m)
{
    if (!m)
    {
        platform_printf("%s: invalid mutex %p\n", __func__, m);
        return;
    }

    os_mutex_delete(&m->mutex);
}


void platform_semaphore_init(Semaphore* s)
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


void platform_semaphore_deinit(Semaphore* s)
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


int platform_semaphore_post(Semaphore* s)
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


int platform_semaphore_wait(Semaphore* s, int timeout_ms)
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

    platform_semaphore_post(&t->join_sem);

    os_thread_self_complete(0);
}


int platform_thread_create(Thread* t, 
        int priority, 
        const char* name, 
        void (*func)(void*), 
        size_t stack_size, 
        void* arg)
{
    if (!t) return -1;

    t->func = func;
    t->arg = arg;

    platform_semaphore_init(&t->join_sem);

    os_thread_stack_define(thread_stack, stack_size);
    int rc = os_thread_create(&t->tid, name, func_wrapper, (os_thread_arg_t)t, &thread_stack, priority);
    if (rc != WM_SUCCESS)
    {
        platform_semaphore_deinit(&t->join_sem);
        platform_printf("%s: failed to create thread: rc = %d\n", __func__, rc);
        return -1;
    }

    return 0;
}


int platform_thread_join(Thread* t, int timeout_ms)
{
    if (!t) return -1;

    if (platform_semaphore_wait(&t->join_sem, timeout_ms) != 0)
    {
        platform_printf("%s: timeout waiting for join\n", __func__);
        return -1;
    }

    return 0;
}


int platform_thread_destroy(Thread* t)
{
    if (!t) return -1;

    if (os_thread_delete(&t->tid) != WM_SUCCESS)
    {
        platform_printf("%s: failed to delete thread\n", __func__);
    }

    platform_semaphore_deinit(&t->join_sem);

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

static uint32_t seed;

int platform_rand()
{
	if (!seed) 
	{
		//srand(os_ticks_get());
		seed = sample_initialise_random_seed();
		srand(seed);
	}

	return rand();
}
