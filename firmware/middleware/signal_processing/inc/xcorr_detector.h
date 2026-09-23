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
 * Works in real time: one call per sample, no buffers to pass around. There are two ways to
 * use it (same as iir_filter):
 *
 * - **Single detector (original API):** XCorrInit(), XCorrProcess(), XCorrGetLastPeak() and
 *   XCorrReset(). The module keeps one detector internally, so only one signal can be processed.
 * - **Detector instances:** declare one xcorr_detector_t per signal and pass it to
 *   XCorrDetectorInit(), XCorrDetectorProcess(), XCorrDetectorGetLastPeak() and
 *   XCorrDetectorReset(). Each instance keeps its own template, configuration and state, so
 *   several signals (e.g. several sensors) can be processed independently.
 *
 * The original API is implemented on top of an internal instance, so it behaves exactly as
 * before.
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
 * | 23/09/2026 | Detector instances (xcorr_detector_t) to process several signals at once. The original API is kept |
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

/**
 * @brief Detector instance: template, configuration and state of one detector. Initialize it
 * with XCorrDetectorInit() before use.
 */
typedef struct {
    float template_unit[XCORR_MAX_TEMPLATE_LEN]; /*!< Template normalized to unit energy */
    float window[2 * XCORR_MAX_TEMPLATE_LEN];    /*!< Signal history, stored twice (window[i] and window[i + len]) so that
                                                      the last len samples are always contiguous starting at window[pos] */
    uint16_t len;              /*!< Template length, 0 = not initialized */
    uint16_t pos;              /*!< Where the next sample is written (= oldest sample of the window) */
    uint16_t filled;           /*!< Samples received so far, up to len */
    float threshold;           /*!< Detection threshold */
    uint16_t peak_window;      /*!< Samples to keep looking for the maximum after crossing the threshold */
    uint16_t refractory;       /*!< Samples to ignore after a detection */
    bool searching;            /*!< Threshold crossed, keeping the maximum of the correlation */
    uint16_t search_left;      /*!< Samples left of peak_window */
    uint16_t refractory_left;  /*!< Samples left of refractory */
    float peak_max;            /*!< Maximum of the correlation during the current search */
    float last_peak;           /*!< Maximum of the correlation during the last detection */
} xcorr_detector_t;
/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief Initialize a detector instance (also clears its state)
 *
 * @param detector  Detector instance
 * @param config    Detector configuration (the template is copied, it does not need to stay in memory)
 * @return true if the configuration is valid, false otherwise
 */
bool XCorrDetectorInit(xcorr_detector_t *detector, const xcorr_config_t *config);

/**
 * @brief Process one new sample of the signal with a detector instance
 *
 * The first template_len - 1 samples only fill the buffer: the correlation is 0 and no event
 * is detected.
 *
 * @param detector      Detector instance (initialized with XCorrDetectorInit())
 * @param sample        New sample of the signal
 * @param correlation   Output: correlation with the template for the last template_len samples
 *                      (can be NULL if it is not needed)
 * @return true if an event was detected on this sample (once per event)
 */
bool XCorrDetectorProcess(xcorr_detector_t *detector, float sample, float *correlation);

/**
 * @brief Correlation value at the peak of the last event detected by a detector instance
 *
 * @param detector  Detector instance
 * @return float Maximum of the correlation during the last detection
 */
float XCorrDetectorGetLastPeak(const xcorr_detector_t *detector);

/**
 * @brief Clear the state of a detector instance (buffer, pending detection and refractory
 * period), keeping its configuration
 *
 * @param detector  Detector instance
 */
void XCorrDetectorReset(xcorr_detector_t *detector);

/**
 * @brief Initialize the detector (single internal detector; also clears its state)
 *
 * @param config    Detector configuration (the template is copied, it does not need to stay in memory)
 * @return true if the configuration is valid, false otherwise
 */
bool XCorrInit(const xcorr_config_t *config);

/**
 * @brief Process one new sample of the signal (single internal detector)
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
 * @brief Correlation value at the peak of the last detected event (single internal detector)
 *
 * @return float Maximum of the correlation during the last detection
 */
float XCorrGetLastPeak(void);

/**
 * @brief Clear the detector's state (buffer, pending detection and refractory period), keeping
 * the configuration (single internal detector)
 */
void XCorrReset(void);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* XCORR_DETECTOR_H_ */

/*==================[end of file]============================================*/
