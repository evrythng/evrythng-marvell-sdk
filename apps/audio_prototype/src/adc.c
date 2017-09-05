#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include "appln_dbg.h"

void adc_thread_routine(os_thread_arg_t data)
{
    while (1) {
        dbg("---------------------");
        os_thread_sleep(os_msec_to_ticks(1000));
    }

	os_thread_self_complete(NULL);
	return;
}
