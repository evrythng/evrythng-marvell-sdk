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
#include <led_indicator.h>


#define BIT_RESOLUTION_FACTOR 32768	/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1800  /* Max input voltage in milliVolts */
#define fft_size 256
#define BASE_FREQUENCIES_NUM 8

static float32_t cmplx_in_out[fft_size*2];
static float32_t mag_out[fft_size];
static uint32_t base_frequencies_cnt;

static int magnitude_stat_cnt;
static float magnitude_stats[fft_size/2];
static float32_t magnitude_weights[fft_size/2];

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
    bool button_was_pressed = false;

    while (true) {

        accum_us = 0;
        accum_cnt = 0;
            
        for (i = 0; i < fft_size*2; i+=2) {
            us = os_get_usec_counter();

            // Real value
            cmplx_in_out[i] = adc_drv_result(adc_dev);
            // Im value
            cmplx_in_out[i+1] = 0;

            us = os_get_usec_counter() - us;
            if (us < 5000) {//filter out overflows
                accum_us += us;
                accum_cnt++;
            }

            /*
               float32_t result = (cmplx_in_out[i] / BIT_RESOLUTION_FACTOR) * (VMAX_IN_mV * 2);
               wmprintf("Iteration %d: count %.2f - %d.%d mV, tool %u us\r\n",
               i, cmplx_in_out[i],
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
                cmplx_in_out, 
                0 /* inverse */, 
                1 /* but inverse ? */);

        /* Process the data through the Complex Magnitude Module for
           calculating the magnitude at each bin */
        arm_cmplx_mag_f32(cmplx_in_out, mag_out, fft_size);


        /* first half of magnitudes is useable, fft_size/2 */

        /* Calculate maxValue and returns corresponding BIN value, ignoring bin[0] */
        float max_magnitude; 
        uint32_t max_bin_index; 
        float match_coef = 0;

        magnitude_stat_cnt++;

        /* filter out first 8 bins */
        /*
        for (int i = 0; i < 8; i++)
            mag_out[i] = 0;
        for (int i = 64; i < fft_size; i++)
            mag_out[i] = 0;
        */
        mag_out[0] = 0;

        for (int i = 0; i < fft_size/2; i++) {

            //arm_max_f32(mag_out, fft_size/2, &max_magnitude, &max_bin_index);
            //mag_out[max_bin_index] = 0; //eliminate 'last' max frequency 

            float correlation = 0;
            if (magnitude_weights[i] > 0) {
                float tmp = mag_out[i] / magnitude_weights[i];
                correlation = tmp > 1 ? 1.0/tmp : tmp;
                wmprintf("%7.2f ", correlation);
            }
            match_coef += correlation;

            /* collect statistics */
            magnitude_stats[i] += mag_out[i];

            /*
            wmprintf("%3d: %7.2f ", 
                    max_bin_index, 
                    (max_bin_index * sampling_freq_hz / 2.0) / (fft_size / 2.0));
                    */
        }

        wmprintf(" match = %2.2f\n\r", match_coef/base_frequencies_cnt);
        if (match_coef/base_frequencies_cnt > .4)
            led_fast_blink(board_led_1());
        else
            led_off(board_led_1());


        /* zero stats, collect statistics if button pressed */
        if (board_button_pressed(board_button_1())) {
            if (!button_was_pressed) {
                button_was_pressed = true;
                memset(magnitude_stats, 0, sizeof magnitude_stats);
                memset(magnitude_weights, 0, sizeof magnitude_weights);
                magnitude_stat_cnt = 0;
                base_frequencies_cnt = 0;
            }
        } else {

            if (button_was_pressed) {
                button_was_pressed = false;

                wmprintf("------------------------------------------- %d\n\r", magnitude_stat_cnt);

                for (int i = 0; i < fft_size/2; i++) {
                    magnitude_weights[i] = (magnitude_stats[i] * 1.0) / (magnitude_stat_cnt * 1.0);
                }

                float32_t m_mag; uint32_t m_index;
                arm_max_f32(magnitude_weights, fft_size/2, &m_mag, &m_index);

                for (int i = 0; i < fft_size/2; i++) {
                    if (magnitude_weights[i] / m_mag < 0.7)
                        magnitude_weights[i] = 0;
                    else {
                        base_frequencies_cnt++;
                        wmprintf("bin %d, mean %2.2f\n\r", 
                                i, magnitude_weights[i]);
                    }
                }
                wmprintf("\n\r------------------------------------------- base frequencies = %d\n\r", base_frequencies_cnt);
            }
        }
    }

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);

exit_thread:
	os_thread_self_complete(NULL);
}
