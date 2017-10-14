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

extern void enqueue_in_use_property_update(bool);
extern void enqueue_last_use_property_update(int);

#define fft_size 512
#define half_fft_size (fft_size/2)
#define MATCH_THRESHOLD .6
#define BASE_FREQUENCY_THRESHOLD .7
#define DETECTION_MIN_TIME 1000
#define DETECTION_RELEASE_TIME 4000
#define SPIKE_DETECTION_RELEASE_TIME 2000
#define SPURIOUS_DETECTION_RELEASE_TIME 400

static void correlate_and_match(float* match_coef);
static void check_and_build_model();
static void read_raw_samples();
static void apply_window();
//static void print_max_mags();
static void process_match_result(float match_coef);
    
static float32_t cmplx_in_out[fft_size*2];
static float32_t magnitude_out[fft_size];
static float sampling_freq_hz;

static float magnitude_stats[half_fft_size];

static os_thread_t adc_thread;
static os_thread_stack_define(adc_thread_stack, 12 * 1024);
static int adc_running;

static uint32_t base_frequencies_cnt = 7; // num of freq - 1
static float32_t magnitude_means[half_fft_size] = {
    [34] = 15000,
    [35] = 15000,
    [36] = 15000,
    [37] = 15000,
    [38] = 15000,
    [39] = 15000,
    [40] = 15000,
    [41] = 15000,
};

static mdev_t *adc_dev;
void adc_thread_routine(os_thread_arg_t);

void adc_thread_start()
{
	if (adc_running)
        return;

    adc_running = 1;

    enqueue_in_use_property_update(false);

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
        adc_running = 0;
    }
}

void adc_thread_stop()
{
    adc_running = 0;
}

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

    while (adc_running) {

        read_raw_samples();

        //apply_window();

        /* Process the data through the CFFT/CIFFT module */
        arm_cfft_f32(&arm_cfft_sR_f32_len512, 
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

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);

exit_thread:
    dbg("exiting %s thread routine...", __func__);  
	os_thread_delete(&adc_thread);
}


void read_raw_samples() 
{
    uint32_t us = 0, 
             accum_us = 0, 
             accum_cnt = 0;

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
    }

    /* calculate mean time used to sample */
    accum_us /= accum_cnt;
    sampling_freq_hz = 1000000/accum_us;
}


static double hamming(double n, double frame_size)
{
    return 0.54 - 0.46 * cos((2 * M_PI * n) / (frame_size - 1));
}


void apply_window()
{
    for (int i = 0; i < fft_size*2; i+=2) {
        cmplx_in_out[i] *= hamming(i/2, fft_size);
    }
}


void correlate_and_match(float* match_coef) 
{
    /* 
     * the very first bin contains signal power
     * filter it out
     */
    for (int i = 0; i < 5; i++)
        magnitude_out[i] = 0;

    *match_coef = 0;

    /* first half of magnitudes is usable, half_fft_size */

    for (int i = 0; i < half_fft_size; i++) {

        float correlation = 0;
        if (magnitude_means[i] > 0) {
            float tmp = magnitude_out[i] / magnitude_means[i];
            //correlation = tmp > 1 ? 1.0/tmp : tmp;
            correlation = tmp > 1 ? 1 : tmp;
#if 1
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
#if 1
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

        //print a couple of max magnitudes
      
        float32_t m_mag, m_mag_next; uint32_t m_index;
        arm_max_f32(magnitude_out, half_fft_size, &m_mag, &m_index);

        for (int i = 0; i < half_fft_size; i++) {
            arm_max_f32(magnitude_out, half_fft_size, &m_mag_next, &m_index);
            if (m_mag_next / m_mag > BASE_FREQUENCY_THRESHOLD)
                wmprintf("[%d] = %6.2f, ", m_index, magnitude_out[m_index]);
            magnitude_out[m_index] = 0;
        }
        wmprintf("\n\r");

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
                    wmprintf("[%d] = %.2f,\n\r", i, magnitude_means[i]);
                }
            }
            wmprintf("-------------- base frequencies num = %d\n\r", base_frequencies_cnt);
        }
    }
}

static bool signal_detected;
unsigned first_time_detected;
unsigned last_time_detected;
static bool in_use;
static bool waiting_spike;

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

            //unsigned long detected_time = os_ticks_to_msec(since_first_detected - since_last_detected);
            unsigned long detected_time = os_ticks_to_msec(since_first_detected);
            enqueue_last_use_property_update(ceil((detected_time*1.0)/1000)); //in seconds
        }
    }
}

