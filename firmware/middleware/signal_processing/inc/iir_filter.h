#ifndef IIR_FILTER_H_
#define IIR_FILTER_H_
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Middelware Middelware
 ** @{ */
/** \addtogroup IIR_Filter IIR Filter
 */

/** \brief Functionalities to design and use filters
 *
 * There are two ways to use this module:
 *
 * - **Single filter (original API):** LowPassInit() / LowPassFilter() and HiPassInit() /
 *   HiPassFilter(). The module keeps one low pass and one high pass filter internally, so they
 *   can only filter one signal at a time.
 * - **Filter instances:** declare one iir_filter_t per filter and pass it to IirLowPassInit() /
 *   IirHiPassInit() and IirFilter(). Each instance keeps its own coefficients and state, so
 *   several signals (e.g. several ADC channels) can be filtered independently.
 *
 * The original API is implemented on top of two internal instances, so it behaves exactly as
 * before.
 *
 * @author Peñalva Albano
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 15/03/2024 | Document creation		                         						|
 * | 23/09/2026 | Filter instances (iir_filter_t) to filter several signals at once. The original API is kept (Josefina Giorgi) |
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdint.h>
/*==================[macros]=================================================*/
#define IIR_MAX_SECTIONS    4   /*!< Max. number of 2nd order sections (8th order filter) */
#define IIR_SOS_COEFFS      5   /*!< Coefficients per 2nd order section */
#define IIR_SOS_DELAY       2   /*!< State (delay line) per 2nd order section */
/*==================[typedef]================================================*/
typedef enum filter_order {
    ORDER_2 = 2,        /*!< 2nd order filter */
    ORDER_4 = 4,        /*!< 4th order filter */
    ORDER_6 = 6,        /*!< 6th order filter */
    ORDER_8 = 8         /*!< 8th order filter */
} filter_order_t;

/**
 * @brief Filter instance: coefficients and state of a Butterworth filter built as a cascade of
 * 2nd order sections. Initialize it with IirLowPassInit() or IirHiPassInit() before use.
 */
typedef struct {
    uint8_t n_sections;                                /*!< Number of 2nd order sections (order / 2) */
    float coeffs[IIR_MAX_SECTIONS][IIR_SOS_COEFFS];    /*!< Coefficients of each section */
    float delay[IIR_MAX_SECTIONS][IIR_SOS_DELAY];      /*!< State of each section */
} iir_filter_t;
/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief Initialize a Butterworth Low Pass Filter instance (its state is reset to zero)
 *
 * @param filter        Filter instance
 * @param sample_frec   Signal's sample frequency
 * @param cut_frec      Filter's cut-off frequency
 * @param order         Filter's order (2, 4, 6 or 8)
 */
void IirLowPassInit(iir_filter_t *filter, float sample_frec, float cut_frec, filter_order_t order);

/**
 * @brief Initialize a Butterworth Hi Pass Filter instance (its state is reset to zero)
 *
 * @param filter        Filter instance
 * @param sample_frec   Signal's sample frequency
 * @param cut_frec      Filter's cut-off frequency
 * @param order         Filter's order (2, 4, 6 or 8)
 */
void IirHiPassInit(iir_filter_t *filter, float sample_frec, float cut_frec, filter_order_t order);

/**
 * @brief Apply a filter instance to a signal array. The state is kept between calls, so it can
 * be called with one sample at a time (signal_lenght = 1) for real time filtering.
 *
 * @param filter            Filter instance (initialized with IirLowPassInit() or IirHiPassInit())
 * @param input_signal      Input signal array
 * @param output_signal     Filtered signal array (can be the same array as input_signal)
 * @param signal_lenght     Number of samples of both signals
 */
void IirFilter(iir_filter_t *filter, float *input_signal, float *output_signal, int16_t signal_lenght);

/**
 * @brief Initialize a Butterworth Low Pass Filter (single internal filter)
 *
 * @param sample_frec   Signal's sample frequency
 * @param cut_frec      Filter's cut-off frequency
 * @param order         Filter's order (2, 4, 6 or 8)
 */
void LowPassInit(float sample_frec, float cut_frec, filter_order_t order);

/**
 * @brief Initialize a Butterworth Hi Pass Filter (single internal filter)
 *
 * @param sample_frec   Signal's sample frequency
 * @param cut_frec      Filter's cut-off frequency
 * @param order         Filter's order (2, 4, 6 or 8)
 */
void HiPassInit(float sample_frec, float cut_frec, filter_order_t order);

/**
 * @brief Apply the low pass filter to a signal array
 *
 * @param input_signal      Input signal array
 * @param output_signal     Filtered signal array
 * @param signal_lenght     Number of samples of both signals
 */
void LowPassFilter(float * input_signal, float * output_signal, int16_t signal_lenght);

/**
 * @brief Apply the hi pass filter to a signal array
 *
 * @param input_signal      Input signal array
 * @param output_signal     Filtered signal array
 * @param signal_lenght     Number of samples of both signals
 */
void HiPassFilter(float * input_signal, float * output_signal, int16_t signal_lenght);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* IIR_FILTER_H_ */

/*==================[end of file]============================================*/
