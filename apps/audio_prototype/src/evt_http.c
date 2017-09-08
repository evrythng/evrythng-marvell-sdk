#include <httpc.h>
#include <appln_dbg.h>
#include <wm_mbedtls_helper_api.h>

static http_session_t handle;
static char url_buf[128];
static const char *url_format = "https://api.evrythng.com/thngs/%s/properties";
static http_resp_t *resp;
static httpc_cfg_t httpc_cfg;
static mbedtls_ssl_config *evt_ctx;

extern char api_key[128];
extern char thng_id[64];

void evt_put(const char* data)
{
	evt_ctx = wm_mbedtls_ssl_config_new(0,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_VERIFY_NONE);

	if (!evt_ctx) {
		dbg("failed to create client context");
		return;
	}

	httpc_cfg.ctx = evt_ctx;

    sprintf(url_buf, url_format, thng_id);

    dbg("Connecting to : %s", url_buf);
    dbg("Data to post: %s", data);

    int len = strlen(data);
    int rv = http_open_session(&handle, url_buf, NULL);
    if (rv != 0) {
        dbg("Open session failed: %s (%d)", url_buf, rv);
        goto exit;
    }

    http_req_t req = {
        .type = HTTP_PUT,
        .resource = url_buf,
        .version = HTTP_VER_1_1,
        .content = data,
        .content_len = len,
    };

    rv = http_prepare_req(handle, &req,
            STANDARD_HDR_FLAGS |
            HDR_ADD_CONTENT_TYPE_JSON);
    if (rv != 0) {
        dbg("Prepare request failed: %d", rv);
        goto exit;
    }

    rv = http_add_header(handle, &req, "Authorization", api_key);
    if (rv != 0) {
        dbg("Add header auth failed: %d", rv);
        goto exit;
    }

    rv = http_send_request(handle, &req);
    if (rv != 0) {
        dbg("Send request failed: %d", rv);
        goto exit;
    }

    rv = http_get_response_hdr(handle, &resp);
    if (rv != 0) {
        dbg("Get resp header failed: %d", rv);
        goto exit;
    }

    http_close_session(&handle);

    dbg("HTTP Request Status code: %d", resp->status_code);

exit:
	wm_mbedtls_ssl_config_free(evt_ctx);
}
