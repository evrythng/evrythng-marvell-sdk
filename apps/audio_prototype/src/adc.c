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


#define fft_size 256
#define half_fft_size (fft_size/2)
#define MATCH_THRESHOLD .45
#define BASE_FREQUENCY_THRESHOLD .6
#define DETECTION_MIN_TIME 1500
#define DETECTION_RELEASE_TIME 3000
#define SPURIOUS_DETECTION_RELEASE_TIME 300

static void correlate_and_match(float* match_coef);
static void check_and_build_model();
static void read_raw_samples();
static void process_match_result(float match_coef);
    
static float32_t cmplx_in_out[fft_size*2];
static float32_t magnitude_out[fft_size];
static float sampling_freq_hz;

static float magnitude_stats[half_fft_size];

static uint32_t base_frequencies_cnt = 8;
static float32_t magnitude_means[half_fft_size] = {
    [12] = 11000, 
    [13] = 15000,
    [14] = 18000, 
    [15] = 17000, 
    [16] = 18000, 
    [17] = 14000, 
    [18] = 13000, 
    [19] = 13000
};
/*
[bin 12: mean 16017.89] 
[bin 13: mean 22214.61] 
[bin 14: mean 24301.22] 
[bin 15: mean 20545.66] 
[bin 16: mean 17442.94]
*/

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

    while (true) {

        read_raw_samples();

        /* Process the data through the CFFT/CIFFT module */
        arm_cfft_f32(&arm_cfft_sR_f32_len256, 
                cmplx_in_out, 
                0 /* inverse */, 
                1 /* but inverse ? */);

        /* Process the data through the Complex Magnitude Module for
           calculating the magnitude at each bin */
        arm_cmplx_mag_f32(cmplx_in_out, magnitude_out, fft_size);

        float match_coef;
        correlate_and_match(&match_coef);

        process_match_result(match_coef);

        check_and_build_model();
    }

    /* should never get here */

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);

exit_thread:
	os_thread_self_complete(NULL);
}


void read_raw_samples() 
{
    uint32_t us = 0, 
             accum_us = 0, 
             accum_cnt = 0;

    //os_disable_all_interrupts();
    //unsigned long st = os_enter_critical_section();

    for (int i = 0; i < fft_size*2; i+=2) {

        us = os_get_usec_counter();

        // Real value
        cmplx_in_out[i] = adc_drv_result(adc_dev);
        // Im value
        cmplx_in_out[i+1] = 0;

        us = os_get_usec_counter() - us;
        if (us < 5000) { //filter out overflows
            accum_us += us;
            accum_cnt++;
        }
        //dbg("-------------%d", us);
    }

    //os_exit_critical_section(st);
    //os_enable_all_interrupts();

    /* calculate mean time used to sample */
    accum_us /= accum_cnt;
    sampling_freq_hz = 1000000/accum_us;
}


void correlate_and_match(float* match_coef) 
{
    /* 
     * the very first bin contains signal power
     * filter it out
     */
    magnitude_out[0] = 0;

    *match_coef = 0;

    /* first half of magnitudes is useable, half_fft_size */

    for (int i = 0; i < half_fft_size; i++) {

        float correlation = 0;
        if (magnitude_means[i] > 0) {
            float tmp = magnitude_out[i] / magnitude_means[i];
            correlation = tmp > 1 ? 1.0/tmp : tmp;
#if 0
            if (correlation > MATCH_THRESHOLD)
                wmprintf("[%d %6.2f %6.2f %2.2f] ", 
                        i,
                        magnitude_out[i],
                        magnitude_means[i],
                        correlation);
#endif
        }
        *match_coef += correlation;

        /* collect statistics */
        magnitude_stats[i] += magnitude_out[i];

#if 0
        wmprintf("%3d: %7.2f ", 
                max_bin_index, 
                (max_bin_index * sampling_freq_hz / 2.0) / (half_fft_size));
#endif
    }
    *match_coef /= base_frequencies_cnt;
    if (*match_coef > MATCH_THRESHOLD)
        dbg("sampling freq = %5.2f  match = %2.2f", sampling_freq_hz, *match_coef);
#if 0
    else 
        wmprintf("\n\r");
#endif
}


void check_and_build_model() 
{
    static bool button_was_pressed;
    static int magnitude_stat_cnt;

    magnitude_stat_cnt++;

    /* zero stats, collect statistics if button pressed */
    if (board_button_pressed(board_button_1())) {
        if (!button_was_pressed) {
            button_was_pressed = true;
            memset(magnitude_stats, 0, sizeof magnitude_stats);
            memset(magnitude_means, 0, sizeof magnitude_means);
            magnitude_stat_cnt = 0;
            base_frequencies_cnt = 0;
        }
    } else {
        if (button_was_pressed) {
            button_was_pressed = false;

            wmprintf("------MODEL------- stat cnt = %d\n\r", magnitude_stat_cnt);

            for (int i = 0; i < half_fft_size; i++) {
                magnitude_means[i] = (magnitude_stats[i] * 1.0) / magnitude_stat_cnt;
            }

            float32_t m_mag; uint32_t m_index;
            arm_max_f32(magnitude_means, half_fft_size, &m_mag, &m_index);

            for (int i = 0; i < half_fft_size; i++) {
                if (magnitude_means[i] / m_mag < BASE_FREQUENCY_THRESHOLD)
                    magnitude_means[i] = 0;
                else {
                    base_frequencies_cnt++;
                    wmprintf("[bin %d: mean %.2f] ", i, magnitude_means[i]);
                }
            }
            wmprintf("\n\r-------------- base frequencies num = %d\n\r", base_frequencies_cnt);
        }
    }
}

extern void enqueue_in_use_property_update(bool);
extern void enqueue_last_use_property_update(int);

static bool signal_detected;
unsigned first_time_detected;
unsigned last_time_detected;
static bool in_use;

void process_match_result(float match_coef)
{
    if (match_coef > MATCH_THRESHOLD) {
        if (!signal_detected) {
            first_time_detected = os_ticks_get();
            signal_detected = true;
        }

        last_time_detected = os_ticks_get();

        unsigned diff = last_time_detected - first_time_detected;
        dbg(":: matched ago: %u ms", os_ticks_to_msec(diff));

        if (os_ticks_to_msec(diff) > DETECTION_MIN_TIME && !in_use) {
            in_use = true;
            led_on(board_led_1());
            enqueue_in_use_property_update(true);
        }

    } else {

        unsigned since_first_detected = os_ticks_get() - first_time_detected;
        unsigned since_last_detected = os_ticks_get() - last_time_detected; 

        if (os_ticks_to_msec(since_last_detected) > SPURIOUS_DETECTION_RELEASE_TIME && !in_use) {
            signal_detected = false;
        }

        if (os_ticks_to_msec(since_last_detected) > DETECTION_RELEASE_TIME && in_use) {

            led_off(board_led_1());
            signal_detected = false;

            in_use = false;
            enqueue_in_use_property_update(false);

            unsigned long detected_time = os_ticks_to_msec(since_first_detected - since_last_detected);
            enqueue_last_use_property_update(ceil((detected_time*1.0)/1000)); //in seconds
        }
    }
}

