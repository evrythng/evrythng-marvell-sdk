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
#include <psm.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>

#include "tests.h"

static os_thread_t app_thread;
static os_thread_stack_define(app_stack, 16 * 1024);

#define EVRYTHNG_GET_TIME_URL "http://api.evrythng.com/time"
#define MAX_DOWNLOAD_DATA 150
#define MAX_URL_LEN 128


/* This task configures Evrythng client and connects to the Evrythng cloud  */
static void evrythng_task()
{
    RunAllTests();

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
	jsontok_t json_tokens[9];
	jobj_t json_obj;
	if (json_init(&json_obj, json_tokens, 10, buf, size) != WM_SUCCESS) {
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


/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_cloud_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
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
			ret = os_thread_create(
					       /* thread handle */
					       &app_thread,
					       /* thread name */
					       "evrythng_tests_thread",
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
