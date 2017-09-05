#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include "appln_dbg.h"
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>

#define SAMPLES	256
#define BIT_RESOLUTION_FACTOR 32768	/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1800  /* Max input voltage in milliVolts */

static uint16_t buffer[SAMPLES];
static mdev_t *adc_dev;

void adc_thread_routine(os_thread_arg_t data)
{
	/* adc initialization */
	if (adc_drv_init(ADC0_ID) != WM_SUCCESS) {
		dbg("Error: Cannot init ADC");
        goto exit_thread;
	}

	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);
	int i = adc_drv_selfcalib(adc_dev, vref_internal);
	if (i == WM_SUCCESS)
		dbg("Calibration successful!");
	else {
		dbg("Calibration failed!");
        goto exit_thread;
    }
	adc_drv_close(adc_dev);

	ADC_CFG_Type config;
	adc_get_config(&config);
    adc_modify_default_config(adcClockDivider, ADC_CLOCK_DIVIDER_1);
    adc_modify_default_config(adcResolution, ADC_RESOLUTION_16BIT);
    adc_modify_default_config(adcGainSel, ADC_GAIN_0P5);
    adc_modify_default_config(adcVrefSource, ADC_VREF_18);

	adc_get_config(&config);
	wmprintf("Modified ADC resolution value to %d\r\n", config.adcResolution);
	wmprintf("Modified ADC gain value to %d\r\n", config.adcGainSel);
	wmprintf("Modified ADC vref value to %d\r\n", config.adcVrefSource);

	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);

    uint32_t us = 0;

    while (1) {
        for (i = 0; i < SAMPLES; i++) {
            us = os_get_usec_counter();
            buffer[i] = adc_drv_result(adc_dev);
            us = os_get_usec_counter() - us;

            float result = ((float)buffer[i] / BIT_RESOLUTION_FACTOR) *
                VMAX_IN_mV * ((float)1/(float)(config.adcGainSel != 0 ?
                            config.adcGainSel : 0.5));

            wmprintf("Iteration %d: count %d - %d.%d mV, tool %u us, frequency\r\n",
                    i, buffer[i],
                    wm_int_part_of(result),
                    wm_frac_part_of(result, 2),
                    us);
        }
    }

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);

exit_thread:
	os_thread_self_complete(NULL);
}
