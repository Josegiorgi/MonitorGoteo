/**
 * @file xcorr_detector.c
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 * @brief Peak detector based on cross-correlation with a template
 * @version 0.1
 * @date 2026-09-19
 *
 */

/*==================[inclusions]=============================================*/
#include "xcorr_detector.h"
#include <math.h>
#include <stddef.h>
/*==================[macros and definitions]=================================*/

/*==================[internal data declaration]==============================*/
static float template_unit[XCORR_MAX_TEMPLATE_LEN]; /* template normalized to unit energy */
/* Signal history, stored twice (window[i] and window[i + len]) so that the last len samples are always
 * contiguous in memory starting at window[pos], without wrapping around the buffer. */
static float window[2 * XCORR_MAX_TEMPLATE_LEN];
static uint16_t len = 0;        /* template length, 0 = not initialized */
static uint16_t pos = 0;        /* where the next sample is written (= oldest sample of the window) */
static uint16_t filled = 0;     /* samples received so far, up to len */

static float threshold = 0.0f;
static uint16_t peak_window = 0;
static uint16_t refractory = 0;

static bool searching = false;  /* threshold crossed, keeping the maximum of the correlation */
static uint16_t search_left = 0;
static uint16_t refractory_left = 0;
static float peak_max = 0.0f;
static float last_peak = 0.0f;
/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/*==================[external functions definition]==========================*/

bool XCorrInit(const xcorr_config_t *config) {
    if (config == NULL || config->template_signal == NULL ||
        config->template_len < 2 || config->template_len > XCORR_MAX_TEMPLATE_LEN) {
        return false;
    }

    float energy = 0.0f;
    for (uint16_t i = 0; i < config->template_len; i++) {
        energy += config->template_signal[i] * config->template_signal[i];
    }
    if (energy <= 0.0f) {
        return false;
    }

    float inv_norm = 1.0f / sqrtf(energy);
    for (uint16_t i = 0; i < config->template_len; i++) {
        template_unit[i] = config->template_signal[i] * inv_norm;
    }

    len = config->template_len;
    threshold = config->threshold;
    peak_window = config->peak_window;
    refractory = config->refractory;
    last_peak = 0.0f;
    XCorrReset();
    return true;
}

bool XCorrProcess(float sample, float *correlation) {
    if (len == 0) {
        if (correlation != NULL) {
            *correlation = 0.0f;
        }
        return false;
    }

    window[pos] = sample;
    window[pos + len] = sample;
    pos++;
    if (pos == len) {
        pos = 0;
    }
    if (filled < len) {
        filled++;
    }

    /* Correlation of the last len samples with the template: window[pos] is the oldest one */
    float corr = 0.0f;
    if (filled == len) {
        const float *last_samples = &window[pos];
        for (uint16_t i = 0; i < len; i++) {
            corr += last_samples[i] * template_unit[i];
        }
    }
    if (correlation != NULL) {
        *correlation = corr;
    }

    if (refractory_left > 0) {
        refractory_left--;
        return false;
    }

    if (searching) {
        if (corr > peak_max) {
            peak_max = corr;
        }
        search_left--;
        if (search_left == 0) {
            searching = false;
            last_peak = peak_max;
            refractory_left = refractory;
            return true;
        }
        return false;
    }

    if (corr > threshold) {
        peak_max = corr;
        if (peak_window == 0) {
            last_peak = corr;
            refractory_left = refractory;
            return true;
        }
        searching = true;
        search_left = peak_window;
    }
    return false;
}

float XCorrGetLastPeak(void) {
    return last_peak;
}

void XCorrReset(void) {
    for (uint16_t i = 0; i < 2 * XCORR_MAX_TEMPLATE_LEN; i++) {
        window[i] = 0.0f;
    }
    pos = 0;
    filled = 0;
    searching = false;
    search_left = 0;
    refractory_left = 0;
    peak_max = 0.0f;
}

/*==================[end of file]============================================*/
