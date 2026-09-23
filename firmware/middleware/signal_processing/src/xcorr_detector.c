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
/* Internal instance used by the original single detector API */
static xcorr_detector_t default_detector;
/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/*==================[external functions definition]==========================*/

bool XCorrDetectorInit(xcorr_detector_t *detector, const xcorr_config_t *config) {
    if (detector == NULL || config == NULL || config->template_signal == NULL ||
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
        detector->template_unit[i] = config->template_signal[i] * inv_norm;
    }

    detector->len = config->template_len;
    detector->threshold = config->threshold;
    detector->peak_window = config->peak_window;
    detector->refractory = config->refractory;
    detector->last_peak = 0.0f;
    XCorrDetectorReset(detector);
    return true;
}

bool XCorrDetectorProcess(xcorr_detector_t *d, float sample, float *correlation) {
    if (d->len == 0) {
        if (correlation != NULL) {
            *correlation = 0.0f;
        }
        return false;
    }

    d->window[d->pos] = sample;
    d->window[d->pos + d->len] = sample;
    d->pos++;
    if (d->pos == d->len) {
        d->pos = 0;
    }
    if (d->filled < d->len) {
        d->filled++;
    }

    /* Correlation of the last len samples with the template: window[pos] is the oldest one */
    float corr = 0.0f;
    if (d->filled == d->len) {
        const float *last_samples = &d->window[d->pos];
        for (uint16_t i = 0; i < d->len; i++) {
            corr += last_samples[i] * d->template_unit[i];
        }
    }
    if (correlation != NULL) {
        *correlation = corr;
    }

    if (d->refractory_left > 0) {
        d->refractory_left--;
        return false;
    }

    if (d->searching) {
        if (corr > d->peak_max) {
            d->peak_max = corr;
        }
        d->search_left--;
        if (d->search_left == 0) {
            d->searching = false;
            d->last_peak = d->peak_max;
            d->refractory_left = d->refractory;
            return true;
        }
        return false;
    }

    if (corr > d->threshold) {
        d->peak_max = corr;
        if (d->peak_window == 0) {
            d->last_peak = corr;
            d->refractory_left = d->refractory;
            return true;
        }
        d->searching = true;
        d->search_left = d->peak_window;
    }
    return false;
}

float XCorrDetectorGetLastPeak(const xcorr_detector_t *detector) {
    return detector->last_peak;
}

void XCorrDetectorReset(xcorr_detector_t *detector) {
    for (uint16_t i = 0; i < 2 * XCORR_MAX_TEMPLATE_LEN; i++) {
        detector->window[i] = 0.0f;
    }
    detector->pos = 0;
    detector->filled = 0;
    detector->searching = false;
    detector->search_left = 0;
    detector->refractory_left = 0;
    detector->peak_max = 0.0f;
}

bool XCorrInit(const xcorr_config_t *config) {
    return XCorrDetectorInit(&default_detector, config);
}

bool XCorrProcess(float sample, float *correlation) {
    return XCorrDetectorProcess(&default_detector, sample, correlation);
}

float XCorrGetLastPeak(void) {
    return XCorrDetectorGetLastPeak(&default_detector);
}

void XCorrReset(void) {
    XCorrDetectorReset(&default_detector);
}

/*==================[end of file]============================================*/
