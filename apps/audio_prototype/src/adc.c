#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include "appln_dbg.h"
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>
#include "arm_math.h"
#include "arm_const_structs.h"


#define BIT_RESOLUTION_FACTOR 32768	/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1800  /* Max input voltage in milliVolts */
#define fft_size 256

static float32_t cmplxInOut[fft_size*2];
static float32_t magOutput[fft_size];
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

    uint32_t us, accum_us, accum_cnt;

    while (1) {

        accum_us = 0;
        accum_cnt = 0;
            
        for (i = 0; i < fft_size*2; i+=2) {
            us = os_get_usec_counter();

            // Real value
            cmplxInOut[i] = adc_drv_result(adc_dev);
            // Im value
            cmplxInOut[i+1] = 0;

            us = os_get_usec_counter() - us;
            if (us < 5000) {//filter out overflows
                accum_us += us;
                accum_cnt++;
            }

            /*
               float32_t result = (cmplxInOut[i] / BIT_RESOLUTION_FACTOR) * (VMAX_IN_mV * 2);
               wmprintf("Iteration %d: count %.2f - %d.%d mV, tool %u us\r\n",
               i, cmplxInOut[i],
               wm_int_part_of(result),
               wm_frac_part_of(result, 2),
               us);
               */
        }

        /* calculate mean time used to sample */
        accum_us /= accum_cnt;
        float sampling_freq_hz = 1000000/accum_us;

        /* Process the data through the CFFT/CIFFT module */
        arm_cfft_f32(&arm_cfft_sR_f32_len256, 
                cmplxInOut, 
                0 /* inverse */, 
                1 /* but inverse ? */);

        /* Process the data through the Complex Magnitude Module for
           calculating the magnitude at each bin */
        arm_cmplx_mag_f32(cmplxInOut, magOutput, fft_size);

        /* first half of magnitudes is useable, fft_size/2 */

        /* Calculates maxValue and returns corresponding BIN value, ignoring bin[0] */
        float maxMagnitude; uint32_t maxIndex;
        arm_max_f32(&magOutput[1], (fft_size/2)-1, &maxMagnitude, &maxIndex);
        dbg("max mag: %8.2f, max idx: %3u, freq: %8.2f, sampling freq: %5.2f", 
                maxMagnitude, maxIndex, 
                (maxIndex*sampling_freq_hz/2.0)/(fft_size/2.0),
                sampling_freq_hz);
    }

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);

exit_thread:
	os_thread_self_complete(NULL);
}
