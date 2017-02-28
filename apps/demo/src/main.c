/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Sample Application demonstrating the use of Evrythng Cloud
 * This application communicates with Evrythng cloud once the device is
 * provisioned. Device can be provisioned using the psm CLIs as mentioned below.
 * After that, it periodically gets/updates (toggles) the state of board_led_1()
 * and board_led_2() from/to the Evrythng cloud.
 */
#include <wm_os.h>
#include <app_framework.h>
#include <httpc.h>
#include <wmtime.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <psm-utils.h>
#include <evrythng/evrythng.h>
#include <led_indicator.h>
#include <push_button.h>


static os_thread_t app_thread;
static os_thread_t button1_thread;
static os_thread_t button2_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 8 * 1024);
static os_thread_stack_define(button_stack, 8 * 1024);

#define EVRYTHNG_GET_TIME_URL "http://api.evrythng.com/time"
#define MAX_DOWNLOAD_DATA 150
#define MAX_URL_LEN 128

static output_gpio_cfg_t led_1;
static output_gpio_cfg_t led_2;
static int button_1;
static int button_2;

os_semaphore_t button1_sem;
os_semaphore_t button2_sem;

evrythng_handle_t evt_handle;
char *thng_id;


/* This is a callback function. It is called when button is pressed or released. */
void pushbutton_cb(int pin, void *data)
{
    if (pin == button_1 && button1_sem != 0)
    {
        os_semaphore_put(&button1_sem);
    }

    if (pin == button_2 && button2_sem != 0)
    {
        os_semaphore_put(&button2_sem);
    }
}


static void on_connection_lost()
{
    wmprintf("connection to cloud lost\n\r");
}


static void on_connection_restored()
{
    wmprintf("connection to cloud restored\n\r");
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


static void action_led_callback(const char* json_str, size_t size)
{
    jobj_t json;
    int err = json_parse_start(&json, (char*)json_str, size);
    if (err != WM_SUCCESS) 
    {
        wmprintf("Wrong json string\n\r");
        return;
    }

    output_gpio_cfg_t led;

	char type[32];
	if (json_get_val_str(&json, "type", type, sizeof(type)) != WM_SUCCESS) 
    {
		wmprintf("type doesn't exist\r\n");
        goto callback_exit;
	}

    if (strcmp(type, "_led1") == 0) 
    {
        led = led_1;
    }
    else if (strcmp(type, "_led2") == 0) 
    {
        led = led_2;
    }
    else
    {
		wmprintf("not expected type value\r\n");
        goto callback_exit;
    }

    if (json_get_composite_object(&json, "customFields") != WM_SUCCESS) 
    {
        wmprintf("Custom fields doesn't exist\n\r");
        goto callback_exit;
    }

    char status[16];
    if (json_get_val_str(&json, "status", status, sizeof(status)) != WM_SUCCESS) 
    {
        wmprintf("Status doesn't exist\n\r");
        goto callback_exit;
    }

    json_release_composite_object(&json);

    wmprintf("Received action: type = \"%s\", status = \"%s\"\n\r", type, status);

    if (strcmp(status, "0") == 0) 
    {
        led_off(led);
    } 
    else if (strcmp(status, "1") == 0) 
    {
        led_on(led);
    }

callback_exit:
    json_parse_stop(&json);
}


/* This task publishes messages to the Evrythng cloud when button is pressed. */
static void button_task(os_thread_arg_t arg)
{
    int led_state = 0;
    int button = (int)arg;
    int presses = 0;

    char json_str[256];
    char led_action_json_fmt[] = "{\"type\":\"%s\",\"customFields\":{\"status\":\"%d\"}}";
    char button_json_fmt[] = "[{\"value\": \"%d\"}]";

    char* button_property = button == button_1 ? "button_1" : "button_2";
    char* led_action = button == button_1 ? "_led1" : "_led2";
    os_semaphore_t sem = button == button_1 ? button1_sem : button2_sem;

    while(1) 
    {
        if (os_semaphore_get(&sem, OS_WAIT_FOREVER) == WM_SUCCESS) 
        {
            sprintf(json_str, button_json_fmt, ++presses);
            EvrythngPubThngProperty(evt_handle, thng_id, button_property, json_str);

            led_state = !led_state;
            sprintf(json_str, led_action_json_fmt, led_action, led_state);

            EvrythngPubThngAction(evt_handle, thng_id, led_action, json_str);
        }
    }
}


/* This task configures Evrythng client and connects to the Evrythng cloud  */
static void evrythng_task()
{
    psm_handle_t handle;
    int rc;

    if ((rc = psm_open(&handle, "evrythng")) != 0) 
    {
        wmprintf("psm_open failed with: %d (Is the module name registered?)\n\r", rc);
        goto exit;
    }

    char api_key[128];
    if (psm_get(&handle, "api_key", api_key, 128) == 0) 
    {
        wmprintf("api_key: %s\n\r", api_key);
    }
    else 
    {
        wmprintf("api_key doesn't exist\n\r");
        goto exit;
    }

    char thng_id_buf[64];
    if (psm_get(&handle, "thng_id", thng_id_buf, 64) == 0) 
    {
        if (thng_id != NULL)
        {
            os_mem_free(thng_id);
        }
        thng_id = (char*)os_mem_alloc((strlen(thng_id_buf)+1)*sizeof(char));
        strcpy(thng_id, thng_id_buf);
        wmprintf("thng_id: %s\n\r", thng_id);
    }
    else 
    {
        wmprintf("thng_id doesn't exist\n\r");
        goto exit;
    }

    char url_buf[64];
    if (psm_get(&handle, "url", url_buf, 64) == 0) 
    {
        wmprintf("Evrythng URL: %s\n\r", url_buf);
    }
    else 
    {
        wmprintf("Evrythng URL doesn't exist\n\r");
        goto exit;
    }
    psm_close(&handle);

    EvrythngInitHandle(&evt_handle);
    EvrythngSetUrl(evt_handle, url_buf);
    EvrythngSetKey(evt_handle, api_key);
    EvrythngSetLogCallback(evt_handle, log_callback);
    EvrythngSetConnectionCallbacks(evt_handle, on_connection_lost, on_connection_restored);

    while ((rc = EvrythngConnect(evt_handle)) != EVRYTHNG_SUCCESS) 
    {
        wmprintf("connection failed (%d), retry\n\r", rc);
        os_thread_sleep(os_msec_to_ticks(5000));
    }
    wmprintf("Connected\n\r");

    os_semaphore_create_counting(&button1_sem, "button1_sem", 1000, 0);
    os_semaphore_create_counting(&button2_sem, "button1_sem", 1000, 0);

    EvrythngSubThngAction(evt_handle, thng_id, "_led1", 0, action_led_callback);
    EvrythngSubThngAction(evt_handle, thng_id, "_led2", 0, action_led_callback);

    EvrythngSubThngProperty(evt_handle, thng_id, "property_1", 0, action_led_callback);
    EvrythngSubThngProperty(evt_handle, thng_id, "property_2", 0, action_led_callback);


    os_thread_create(&button1_thread, "button1_task", button_task, (void*)button_1, &button_stack, OS_PRIO_3);
    os_thread_create(&button2_thread, "button2_task", button_task, (void*)button_2, &button_stack, OS_PRIO_3);

exit:
    os_thread_self_complete(0);
}


static void set_device_time()
{
	http_session_t handle;
	http_resp_t *resp = NULL;
	char buf[MAX_DOWNLOAD_DATA];
	int64_t timestamp, offset;
	int size = 0;
	int status = 0;
	char url_name[MAX_URL_LEN];

	memset(url_name, 0, sizeof(url_name));
	strncpy(url_name, EVRYTHNG_GET_TIME_URL, strlen(EVRYTHNG_GET_TIME_URL));

	wmprintf("Get time from : %s\r\n", url_name);
	status = httpc_get(url_name, &handle, &resp, NULL);
	if (status != WM_SUCCESS) {
		wmprintf("Getting URL failed");
		return;
	}
	size = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
	if (size <= 0) {
		wmprintf("Reading time failed\r\n");
		goto out_time;
	}
	/*
	  If timezone is present in PSM
	  Get on http://api.evrythng.com/time?tz=<timezone>
	  The data will look like this
	  {
	  "timestamp":1429514751927,
	  "offset":-18000000,
	  "localTime":"2015-04-20T02:25:51.927-05:00",
	  "nextChange":1446361200000
	  }
	  If timezone is not presentin PSM
	  Get on http://api.evrythng.com/time
	  The data will look like this
	  {
	  "timestamp":1429514751927
	  }
	*/
	jobj_t json_obj;
	if (json_parse_start(&json_obj, buf, size) != WM_SUCCESS) {
		wmprintf("Wrong json string\r\n");
		goto out_time;
	}

	if (json_get_val_int64(&json_obj, "timestamp", &timestamp) == WM_SUCCESS) {
		if (json_get_val_int64(&json_obj, "offset", &offset)
		    != WM_SUCCESS) {
			offset = 0;
		}
		timestamp = timestamp + offset;
	}
	wmtime_time_set_posix(timestamp/1000);

    json_parse_stop(&json_obj);

out_time:
	http_close_session(&handle);
	return;
}


/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 *
 */
void appln_critical_error_handler(void *data)
{
	while (1)
		;
	/* do nothing -- stall */
}


/* Configure GPIO pins to be used as LED and push button */
static void configure_gpios()
{
	/* Get the corresponding pin numbers using the board specific calls */
	/* also configures the gpio accordingly for LED */
	led_1 = board_led_1();
	led_2 = board_led_2();
	button_1 = board_button_1();
	button_2 = board_button_2();

    push_button_set_cb((input_gpio_cfg_t){button_1, GPIO_ACTIVE_LOW}, pushbutton_cb, 0, 0, 0);
    push_button_set_cb((input_gpio_cfg_t){button_2, GPIO_ACTIVE_LOW}, pushbutton_cb, 0, 0, 0);
}



/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_cloud_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init(sys_psm_get_handle(), NULL);
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;

		if (i == APP_NETWORK_NOT_PROVISIONED) {
			wmprintf("\r\nPlease provision the device "
				 "and then reboot it:\r\n\r\n");
			wmprintf("psm-set network ssid <ssid>\r\n");
			wmprintf("psm-set network security <security_type>"
				 "\r\n");
			wmprintf("    where: security_type: 0 if open,"
				 " 3 if wpa, 4 if wpa2\r\n");
			wmprintf("psm-set network passphrase <passphrase>"
				 " [valid only for WPA and WPA2 security]\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf("pm-reboot\r\n\r\n");
		} else
			app_sta_start();

		break;
	case AF_EVT_NORMAL_CONNECTED:
		set_device_time();
		if (!is_cloud_started) {
			configure_gpios();
			os_thread_sleep(2000);
			ret = os_thread_create(
					       /* thread handle */
					       &app_thread,
					       /* thread name */
					       "evrythng_demo_thread",
					       /* entry function */
					       evrythng_task,
					       /* argument */
					       0,
					       /* stack */
					       &app_stack,
					       /* priority - medium low */
					       OS_PRIO_3);
			is_cloud_started = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmstdio_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wlan_cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return;
}

int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return 0;
}
