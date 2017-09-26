#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include "appln_dbg.h"
#include <psm.h>
#include <psm-utils.h>
#include <evrythng/evrythng.h>
#include "evrythng/platform.h"
#include <work-queue.h>
#include <system-work-queue.h>


static evrythng_handle_t evt_handle;
static char api_key[128];
static char thng_id[64];
static char product_id[64];
static char json_str[512];
static uint64_t con_lost_ticks;

static int latency_check(void*);
static wq_job_t latency_check_job = {
    .job_func = latency_check,
    .periodic_ms = 6*60*1000,
    .initial_delay_ms = 0,
    .param = 0
};
static job_handle_t latency_check_job_handle;

static int connection_update(void*);
static wq_job_t connection_update_job = {
    .job_func = connection_update,
    .periodic_ms = 0,
    .initial_delay_ms = 0,
    .param = 0
};

uint64_t offline_ticks;

/* statistics variables */

static uint32_t reconnection_count;
static uint32_t reconnection_time;
static int32_t pub_thng_prop_time;
static int32_t pub_thng_action_time;
static int32_t pub_product_prop_time;
static int32_t pub_product_action_time;
static char    uptime[64]; //string in a form of: "NNh XXm YYms"
static char    online[64]; //string in a form of: "NNh XXm YYms"

/************************/
static char* random_prop = "random";

static char* random_prop_format = "[{\"key\": \"random\", \"value\": %u}]";

static char* action_name = "_action_1";

static char* action_format = "{\"type\": \"_action_1\"}";

static char* uptime_format = "%uh %um %us";

static char* format1 = "[{\"key\": \"reconnection_time\", \"value\": %u},"
                         "{\"key\": \"reconnection_count\", \"value\": %u}]";

static char* format2 = "[{\"key\": \"uptime\", \"value\": \"%s\"},"
                         "{\"key\": \"online\", \"value\": \"%s\"},"
                         "{\"key\": \"pub_thng_prop_time\", \"value\": %d},"
                         "{\"key\": \"pub_thng_action_time\", \"value\": %d},"
                         "{\"key\": \"pub_product_prop_time\", \"value\": %d},"
                         "{\"key\": \"pub_product_action_time\", \"value\": %d},"
                         "{\"key\": \"free_ram\", \"value\": %zu}]";


static void refresh_uptime() 
{
    uint64_t t = os_total_ticks_get() * (portTICK_RATE_MS);
    uint32_t h = t / (1000*60*60);
    uint32_t m = (t - h*1000*60*60) / (1000*60);
    uint32_t s = (t - h*1000*60*60 - m*1000*60) / (1000);
    sprintf(uptime, uptime_format, h, m, s);
}

static void refresh_online() 
{
    uint64_t t = (os_total_ticks_get() - offline_ticks) * (portTICK_RATE_MS);
    uint32_t h = t / (1000*60*60);
    uint32_t m = (t - h*1000*60*60) / (1000*60);
    uint32_t s = (t - h*1000*60*60 - m*1000*60) / (1000);
    sprintf(online, uptime_format, h, m, s);
}


int evt_init()
{
    psm_handle_t handle;
    int rc;

    if ((rc = psm_open(&handle, "evrythng")) != 0) {
        dbg("psm_open failed with: %d (Is the module name registered?)", rc);
        return rc;
    }

    if ((rc = psm_get(&handle, "api_key", api_key, sizeof api_key)) == 0) {
        dbg("evt api_key: %s", api_key);
    } else {
        dbg("api_key doesn't exist");
        goto exit;
    }

    if ((rc = psm_get(&handle, "thng_id", thng_id, sizeof thng_id)) == 0) {
        dbg("evt thng_id: %s", thng_id);
    } else {
        dbg("thng_id doesn't exist");
        goto exit;
    }

    if ((rc = psm_get(&handle, "product_id", product_id, sizeof product_id)) == 0) {
        dbg("evt product_id: %s", product_id);
    } else {
        dbg("product_id doesn't exist");
        goto exit;
    }

exit:
    psm_close(&handle);
    return rc;
}


static void log_callback(evrythng_log_level_t level, const char* fmt, va_list vl)
{
    char msg[128];

    unsigned n = vsnprintf(msg, sizeof msg, fmt, vl);
    if (n >= sizeof msg)
        msg[sizeof msg - 1] = '\0';

    switch (level)
    {
        case EVRYTHNG_LOG_ERROR:
            wmprintf("ERROR: ");
            break;
        case EVRYTHNG_LOG_WARNING:
            wmprintf("WARNING: ");
            break;
        default:
        case EVRYTHNG_LOG_DEBUG:
            wmprintf("DEBUG: ");
            break;
    }
    wmprintf("%s\n\r", msg);
}


static void on_connection_lost()
{
    dbg("evt connection lost");
    con_lost_ticks = os_total_ticks_get();
    int r = work_dequeue(sys_work_queue_get_handle(), 
            &latency_check_job_handle);
    if (r < 0)
        dbg("work_dequeue on connection lost failed: -%d", -r);

}


#define elapsed_ms(since) (((os_total_ticks_get()) - since) * (portTICK_RATE_MS))


static void on_connection_restored()
{
    dbg("evt connection restored");

    reconnection_time = elapsed_ms(con_lost_ticks);

    offline_ticks += reconnection_time;

    int r = work_enqueue(sys_work_queue_get_handle(), 
            &connection_update_job, 0);
    if (r < 0)
        dbg("work_enqueue connection_update_job on restored failed: -%d", -r);

    r = work_enqueue(sys_work_queue_get_handle(), 
            &latency_check_job, 
            &latency_check_job_handle);
    if (r < 0)
        dbg("work_enqueue latency_check_job on restored failed: -%d", -r);
}


int latency_check(void* unused)
{
    uint64_t t = os_total_ticks_get();

    sprintf(json_str, random_prop_format, platform_rand());
    int r = EvrythngPubThngProperty(evt_handle, thng_id, random_prop, json_str);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish thng property in %s: -%d", __func__, -r);
        pub_thng_prop_time = r;
    } else {
        pub_thng_prop_time = elapsed_ms(t);
    }


    t = os_total_ticks_get();
    r = EvrythngPubThngAction(evt_handle, thng_id, action_name, action_format);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish thng action in %s: -%d", __func__, -r);
        pub_thng_action_time = r;
    } else {
        pub_thng_action_time = elapsed_ms(t);
    }


    t = os_total_ticks_get();
    sprintf(json_str, random_prop_format, platform_rand());
    r = EvrythngPubProductProperty(evt_handle, product_id, random_prop, json_str);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish product property in %s: -%d", __func__, -r);
        pub_product_prop_time = r;
    } else {
        pub_product_prop_time = elapsed_ms(t);
    }

    t = os_total_ticks_get();
    r = EvrythngPubProductAction(evt_handle, product_id, action_name, action_format);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish product action in %s: -%d", __func__, -r);
        pub_product_action_time = r;
    } else {
        pub_product_action_time = elapsed_ms(t);
    }
    
    refresh_uptime();
    refresh_online();

    const heapAllocatorInfo_t* s = getheapAllocInfo();

    sprintf(json_str, format2, uptime, online,
            pub_thng_prop_time,
            pub_thng_action_time,
            pub_product_prop_time,
            pub_product_action_time,
            s->freeSize);
            
    r = EvrythngPubThngProperties(evt_handle, thng_id, json_str);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish stat properties in %s: -%d", __func__, -r);
    }

    return r;
}


int connection_update(void* unused)
{
    sprintf(json_str, format1, reconnection_time, reconnection_count++);
    int r = EvrythngPubThngProperties(evt_handle, thng_id, json_str);
    if (r != EVRYTHNG_SUCCESS) {
        dbg("failed to pubish properties in %s: -%d", __func__, -r);
    }
    return r;
}


void lt_thread_routine(os_thread_arg_t data)
{
    if (evt_init() != 0) {
        dbg("failed to read evt credentials");
        goto exit_thread;
    }

    int rc;

    EvrythngInitHandle(&evt_handle);
    EvrythngSetUrl(evt_handle, "ssl://mqtt.evrythng.com:443");
    EvrythngSetKey(evt_handle, api_key);
    EvrythngSetThreadPriority(evt_handle, OS_PRIO_3);
    EvrythngSetLogCallback(evt_handle, log_callback);
    EvrythngSetConnectionCallbacks(evt_handle, on_connection_lost, on_connection_restored);

    uint64_t t = os_total_ticks_get();

    while ((rc = EvrythngConnect(evt_handle)) != EVRYTHNG_SUCCESS) {
        dbg("connection failed (%d), retry", rc);
        os_thread_sleep(os_msec_to_ticks(5000));
    }
    dbg("EVT Connected\n\r");

    offline_ticks = os_total_ticks_get();

    reconnection_time = elapsed_ms(t);

    int r = work_enqueue(sys_work_queue_get_handle(), 
            &connection_update_job, 0);
    if (r < 0)
        dbg("work_enqueue connection_update_job on startup failed: -%d", -r);

    r = work_enqueue(sys_work_queue_get_handle(), 
            &latency_check_job, 
            &latency_check_job_handle);
    if (r < 0)
        dbg("work_enqueue latency_check_job on startup failed: -%d", -r);

    while (1)
        os_thread_sleep(os_msec_to_ticks(1000));

exit_thread:
	os_thread_self_complete(NULL);
}
