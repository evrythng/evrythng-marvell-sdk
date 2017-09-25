#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include "appln_dbg.h"
#include <psm.h>
#include <psm-utils.h>
#include <evrythng/evrythng.h>
#include <work-queue.h>
#include <system-work-queue.h>


evrythng_handle_t evt_handle;
char api_key[128];
char thng_id[64];
char product_id[64];


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
}


static void on_connection_restored()
{
    dbg("evt connection restored");
}

#if 0
static int update_in_use_property(void* p_in_use)
{
    bool in_use = (bool)p_in_use;
    char json_str[256];
    char value_json_fmt[] = "[{\"key\": \"in_use\", \"value\": %s}]";
    sprintf(json_str, value_json_fmt, in_use ? "true" : "false");
    //return EvrythngPubThngProperty(evt_handle, thng_id, "in_use", json_str);
    return evt_put(json_str);
}


static int update_last_use_property(void* p_last_use)
{
    int last_use = (int)p_last_use;
    char json_str[256];
    char value_json_fmt[] = "[{\"key\": \"last_use\", \"value\": %d}]";
    sprintf(json_str, value_json_fmt, last_use);
    //return EvrythngPubThngProperty(evt_handle, thng_id, "last_use", json_str);
    return evt_put(json_str);
}

void enqueue_in_use_property_update(bool in_use)
{
    wq_job_t job = {
        .job_func = update_in_use_property,
        .periodic_ms = 0,
        .initial_delay_ms = 0,
        .param = (void*)in_use,
    };
    work_enqueue(sys_work_queue_get_handle(), &job, NULL);
}

void enqueue_last_use_property_update(int last_use)
{
    wq_job_t job = {
        .job_func = update_last_use_property,
        .periodic_ms = 0,
        .initial_delay_ms = 0,
        .param = (void*)last_use,
    };
    work_enqueue(sys_work_queue_get_handle(), &job, NULL);
}
#endif


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

    uint64_t t1 = os_total_ticks_get();

    while ((rc = EvrythngConnect(evt_handle)) != EVRYTHNG_SUCCESS) 
    {
        dbg("connection failed (%d), retry", rc);
        os_thread_sleep(os_msec_to_ticks(5000));
    }
    dbg("EVT Connected\n\r");

    uint64_t t2 = os_total_ticks_get();

    dbg("time to connect: %u", (t2-t1) * portTICK_RATE_MS);


exit_thread:
	os_thread_self_complete(NULL);
}
