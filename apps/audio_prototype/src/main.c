/*
 *  Copyright (C) 2008-2017, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Simple Provisioning Application
 *
 * Summary:
 *
 * This application uses the Application Framework to perform network
 * provisioning of the device. It showcases different combinations of
 * security settings at various levels in micro-AP mode provisioning.
 * Wi-Fi level security: Micro-AP level Open or WPA2-PSK security
 * Network level security: HTTP or HTTPS Server support
 * Application level Security: Pin based security support in provisioning app
 *
 * The default provisioning configuration is, micro-AP with WPA2-PSK security,
 * HTTP server and application level security disabled.
 * Application developer can configure different combinations of security
 * settings by modifying the configuration variables in application's build.mk.
 *
 * Micro-AP Network Credentials:
 *
 *  Network name: Marvell_Prov_Demo
 *  Network Passphrase: marvellwm
 *
 * Description:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * The DNS redirector is started, with a list of domain names to resolve.
 * End users can access the device by using one of the configured domain names.
 * The mDNS/DNS-SD service and the Web Server is started.
 *
 * If network configuration is not found, the app starts a micro-AP network
 * along with a DHCP server. This will create a WLAN network and also creates
 * a IP network interface associated with that WLAN network. The DHCP server
 * allows devices to associate and connect to this network and establish an
 * IP level connection.
 *
 * When the network is initialized and ready, app will receive a
 * micro-AP started event.
 *
 * Micro-AP Started:
 *
 * The Provisioning service is then started. This service allows users to
 * perform network provisioning using one of the following methods:
 *
 * 1. Webapp-based Provisioning: Users can connect to the micro-AP network and
 * access the Webapp that is served by the Web Server. The Webapp has a wizard
 * that takes the users through the network provisioning steps
 *
 * Once network provisioning is successful, the network credentials are recorded
 * in the PSM (Persistent Storage Manager). From next bootup on wards, once the
 * WLAN subsystem is initialized, the application knows that the network
 * credentials are already present, and the application directly starts the
 * station interface.
 *
 */
#include <wm_os.h>
#include <wmtime.h>
#include <wmsdk.h>
#include <app_framework.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <psm-utils.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <reset_prov_helper.h>
#include <httpd.h>
#include <ftfs.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <mdev_gpio.h>
#include <critical_error.h>

/*-----------------------Global declarations----------------------*/

appln_config_t appln_cfg = {
	.ssid = "Audio_Sensing_Prototype",
	.passphrase = "marvellwm",
	.hostname = "provdemo",
};

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

static uint8_t first_time = 1;
struct fs *fs;

/* Thread handle */
static os_thread_t adc_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(adc_thread_stack, 12 * 1024);

/*-----------------------Global declarations----------------------*/
static int provisioned;

/* This function must initialize the variables required (network name,
 * passphrase, etc.) It should also register all the event handlers that are of
 * interest to the application.
 */
int appln_config_init()
{
	/* Initialize service name for mdns */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, "audiosensor");
	/* Initialize reset to provisioning push button settings */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
	return 0;
}

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 */
void critical_error(int crit_errno, void *data)
{
	dbg("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

static int appln_httpd_with_fs_start()
{
	int ret;
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Failed to start HTTPD");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 * When WLAN is started, the application framework looks to
 * see whether a home network information is configured
 * and stored in PSM (persistent storage module).
 *
 * The data field returns whether a home network is provisioned
 * or not, which is used to determine what network interfaces
 * to start (station, micro-ap, or both).
 *
 * If provisioned, the station interface of the device is
 * connected to the configured network.
 *
 * Else, Micro-AP network is configured.
 *
 * (If desired, the Micro-AP network can also be started
 * along with the station interface.)
 *
 * We also start all the services which don't need to be
 * restarted between provisioned and non-provisioned mode
 * or between connected and disconnected state.
 *
 * Accordingly:
 *	-- Start mDNS and advertize services
 *	-- Start HTTP Server
 *	-- Register WSGI handlers for HTTP server
 */
#ifdef UAP_PROV_CONFIG_HTTPS_ENABLE
static int load_tls_cert_to_buffer(struct fs *fs,
				   const char *filename, void **buffer)
{
	dbg("%s: %s", __func__, filename);
	int cert_sz = ftfs_get_filesize(fs, filename);
	if (cert_sz <= 0) {
		return -WM_FAIL;
	}

	dbg("%s cert size is %d", __func__, cert_sz);
	void *cert_buf = os_mem_alloc(cert_sz);
	if (!cert_sz)
		return -WM_E_NOMEM;

	int rv = ftfs_load_entire_file_to_buffer(fs, filename,
						 cert_buf, cert_sz);
	if (rv <= 0) {
		os_mem_free(cert_buf);
		return -WM_FAIL;
	}

	*buffer = cert_buf;
	return rv;
}
#endif /* UAP_PROV_CONFIG_HTTPS_ENABLE */

void event_wlan_init_done(void *data)
{
	int ret = -WM_FAIL;
	/* We receive provisioning status in data */
	provisioned = (int)data;

	dbg("AF_EVT_WLAN_INIT_DONE provisioned=%d", provisioned);

	if (provisioned) {
		app_sta_start();
	} else {
		char *domain_names[] = {"provdemo.net", "www.provdemo.net",
					NULL};
		dhcp_enable_dns_server(domain_names);
		app_uap_start_with_dhcp(appln_cfg.ssid, NULL);
	}

	if (provisioned)
		hp_configure_reset_prov_pushbutton();

	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	hp_mdns_start();

#ifdef UAP_PROV_CONFIG_HTTPS_ENABLE
	app_ftfs_init(ftfs_api_version, ftfs_part_name, &fs);

	void *server_cert_buf = NULL, *server_key_buf = NULL;
	int server_cert_sz, server_key_sz;

	server_cert_sz = load_tls_cert_to_buffer(fs, "server_cert.pem",
						 &server_cert_buf);
	if (server_cert_sz <= 0) {
		dbg("Error: No valid server certificate found");
		return;
	}

	server_key_sz = load_tls_cert_to_buffer(fs, "server_key.pem",
						&server_key_buf);
	if (server_key_sz <= 0) {
		dbg("Error: No valid server key found");
		return;
	}

	httpd_tls_certs_t httpd_tls_certs = {
		.server_cert = server_cert_buf,
		.server_cert_size = server_cert_sz,
		.server_key = server_key_buf,
		.server_key_size = server_key_sz,
		.client_cert = NULL,
		.client_cert_size = 0,
	};
	ret = httpd_use_tls_certificates(&httpd_tls_certs);
	if (server_key_buf) {
		os_mem_free(server_key_buf);
		server_key_buf = NULL;
	}
	if (server_cert_buf) {
		os_mem_free(server_cert_buf);
		server_cert_buf = NULL;
	}

	if (ret != WM_SUCCESS) {
		dbg("Error: https session failed");
		return;
	}

#endif /* UAP_PROV_CONFIG_HTTPS_ENABLE */
	/*
	 * Start http server and enable webapp in the
	 * FTFS partition on flash
	 */
	if(appln_httpd_with_fs_start() != WM_SUCCESS)
		return;
	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- ftfs: allows user to see contents of ftfs
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = psm_cli_init(sys_psm_get_handle(), NULL);
	if (ret != WM_SUCCESS)
		dbg("Error: psm_cli_init failed");
	ret = ftfs_cli_init(fs);
	if (ret != WM_SUCCESS)
		dbg("Error: ftfs_cli_init failed");
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	if (!provisioned) {
		/* Start Slow Blink */
		led_slow_blink(board_led_2());
	}

}

/*
 * Event: Micro-AP Started
 *
 * If we are not provisioned, then start provisioning on
 * the Micro-AP network.
 *
 * Since Micro-AP interface is UP, announce mDNS service
 * on the Micro-AP interface.
 */
void event_uap_started(void *data)
{
	void *iface_handle = net_get_uap_handle();

	if (!provisioned) {
		dbg("Starting provisioning");
		app_provisioning_start(PROVISIONING_WLANNW);
	}
	dbg("Event: Micro-AP Started");
	hp_mdns_announce(iface_handle);
}

void event_normal_reset_prov(void *data)
{
	led_on(board_led_2());
	first_time = 1;

	/* Start Slow Blink */
	led_slow_blink(board_led_2());

	/* Reset to provisioning */
	provisioned = 0;
	hp_unconfigure_reset_prov_pushbutton();
	if (is_uap_started() == false) {
		char *domain_names[] = {"provdemo.net", "www.provdemo.net",
					NULL};
		dhcp_enable_dns_server(domain_names);
		app_uap_start_with_dhcp(appln_cfg.ssid, NULL);
	} else {
		app_provisioning_start(PROVISIONING_WLANNW);
	}
}

void event_prov_done(void *data)
{
	hp_configure_reset_prov_pushbutton();
	app_provisioning_stop();
	dbg("Provisioning successful");
}

void event_prov_client_done(void *data)
{
	app_uap_stop();
	dhcp_server_stop();
}

void event_normal_connecting(void *data)
{
	net_dhcp_hostname_set(appln_cfg.hostname);
	dbg("Connecting to Home Network");
	/* Start Fast Blink */
	led_fast_blink(board_led_2());
}

extern void adc_thread_routine(os_thread_arg_t);

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to home AP.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
void event_normal_connected(void *data)
{
	void *iface_handle;
	char ip[16];

	led_off(board_led_2());

	app_network_ip_get(ip);
	dbg("Connected to provisioned network with ip address =%s", ip);

	iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle);

    dbg("Ready for operation");

	if (!adc_thread) {
		/* set system time */
        /* TODO */

		/* create main thread */
		int ret = os_thread_create(&adc_thread,
			/* thread name */
			"adc_thread",
			/* entry function */
			adc_thread_routine,
			/* argument */
			0,
			/* stack */
			&adc_thread_stack,
			/* priority */
			OS_PRIO_3);
		if (ret != WM_SUCCESS) {
			dbg("Failed to start adc_thread: %d", ret);
			return;
		}
	}
}

/* Event handler for AF_EVT_NORMAL_DISCONNECTED - Station interface
 * disconnected.
 * Stop the network services which need not be running in disconnected mode.
 */
void event_normal_user_disconnect(void *data)
{
	led_on(board_led_2());
	/* restart the micro-ap interface */
}

void event_normal_link_lost(void *data)
{
	/* is this automatically retried ?? */
	event_normal_user_disconnect(data);
}

void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	/* Reannounce service everytime any resources record changes. In this
	 * case, IP address. */
	hp_mdns_reannounce(iface_handle);
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		event_normal_link_lost(data);
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		event_normal_user_disconnect(data);
		break;
	case AF_EVT_NORMAL_DHCP_RENEW:
		event_normal_dhcp_renew(data);
		break;
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
        critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize CLI Commands
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	appln_config_init();

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
