#ifndef XCORR_DETECTOR_H_
#define XCORR_DETECTOR_H_
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Middelware Middelware
 ** @{ */
/** \addtogroup XCorr_Detector XCorr Detector
 */

/** \brief Peak detector based on cross-correlation with a template, sample by sample
 *
 * Slides a template (the shape of the event to look for, for example an average of several real
 * drops) along the input signal and, on every new sample, computes how much the last
 * template_len samples look like it. Where the event happens the correlation shows a big peak;
 * where there is only noise it stays small. The event is detected when that peak goes over a
 * threshold.
 *
 * The template is normalized internally to unit energy, so the correlation is in the same
 * units as the input signal and keeps its amplitude (a big event gives a big peak). The
 * threshold has to be chosen in those units, for example a multiple of the correlation's
 * standard deviation over a signal without events.
 *
 * Works in real time: one call per sample, no buffers to pass around. The state is kept in
 * static variables, so only one detector can be used at a time (same as iir_filter).
 *
 * @note Templates with positive and negative lobes (as a band-passed pulse) also have side
 * lobes in the correlation, before and after the main peak. To avoid detecting the same event
 * more than once, after crossing the threshold the detector waits peak_window samples keeping
 * the maximum, and then ignores the input for refractory samples.
 *
 * @author Josefina Giorgi
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 19/09/2026 | Document creation		                         						|
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include <stdint.h>
/*==================[macros]=================================================*/
#define XCORR_MAX_TEMPLATE_LEN  128 /*!< Maximum template length, in samples */
/*==================[typedef]================================================*/
/**
 * @brief Detector configuration
 */
typedef struct {
    const float *template_signal; /*!< Shape of the event to look for (any scale, it is normalized inside) */
    uint16_t template_len;        /*!< Number of samples of the template (2 to XCORR_MAX_TEMPLATE_LEN) */
    float threshold;              /*!< Correlation value from which an event is considered (same units as the signal) */
    uint16_t peak_window;         /*!< Samples to wait, after crossing the threshold, keeping the maximum of the correlation */
    uint16_t refractory;          /*!< Samples to ignore the input after a detection */
} xcorr_config_t;
/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief Initialize the detector (also clears its state)
 *
 * @param config    Detector configuration (the template is copied, it does not need to stay in memory)
 * @return true if the configuration is valid, false otherwise
 */
bool XCorrInit(const xcorr_config_t *config);

/**
 * @brief Process one new sample of the signal
 *
 * The first template_len - 1 samples only fill the buffer: the correlation is 0 and no event
 * is detected.
 *
 * @param sample        New sample of the signal
 * @param correlation   Output: correlation with the template for the last template_len samples
 *                      (can be NULL if it is not needed)
 * @return true if an event was detected on this sample (once per event)
 */
bool XCorrProcess(float sample, float *correlation);

/**
 * @brief Correlation value at the peak of the last detected event
 *
 * @return float Maximum of the correlation during the last detection
 */
float XCorrGetLastPeak(void);

/**
 * @brief Clear the detector's state (buffer, pending detection and refractory period), keeping the configuration
 */
void XCorrReset(void);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* XCORR_DETECTOR_H_ */

/*==================[end of file]============================================*/
