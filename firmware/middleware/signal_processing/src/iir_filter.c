/**
 * @file iir_filter.c
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 * @brief
 * @version 0.1
 * @date 2024-03-15
 *
 * @copyright Copyright (c) 2023
 *
 */

/*==================[inclusions]=============================================*/
#include <string.h>
#include "iir_filter.h"
#include "esp_dsp.h"
/*==================[macros and definitions]=================================*/
// 2nd order Butterworth
#define ORDER2_Q    (1 / 1.414)
// 4th order Butterworth
#define ORDER4_Q1   (1 / 0.765)
#define ORDER4_Q2   (1 / 1.848)
// 6th order Butterworth
#define ORDER6_Q1   (1 / 0.518)
#define ORDER6_Q2   (1 / 1.414)
#define ORDER6_Q3   (1 / 1.932)
// 8th order Butterworth
#define ORDER8_Q1   (1 / 0.390)
#define ORDER8_Q2   (1 / 1.111)
#define ORDER8_Q3   (1 / 1.663)
#define ORDER8_Q4   (1 / 1.962)
/*==================[internal data declaration]==============================*/
typedef esp_err_t (*biquad_gen_t)(float *coeffs, float f, float qFactor);
/*==================[internal functions declaration]=========================*/
static void IirInit(iir_filter_t *filter, biquad_gen_t gen, float sample_frec, float cut_frec, filter_order_t order);
/*==================[internal data definition]===============================*/
/* Q factor of each 2nd order section, for each filter order */
static const float q_order2[] = {ORDER2_Q};
static const float q_order4[] = {ORDER4_Q1, ORDER4_Q2};
static const float q_order6[] = {ORDER6_Q1, ORDER6_Q2, ORDER6_Q3};
static const float q_order8[] = {ORDER8_Q1, ORDER8_Q2, ORDER8_Q3, ORDER8_Q4};

/* Internal instances used by the original single filter API */
static iir_filter_t lp_filter;
static iir_filter_t hp_filter;
/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/
static void IirInit(iir_filter_t *filter, biquad_gen_t gen, float sample_frec, float cut_frec, filter_order_t order){
    const float *q;
    switch(order){
        case ORDER_2: q = q_order2; break;
        case ORDER_4: q = q_order4; break;
        case ORDER_6: q = q_order6; break;
        case ORDER_8: q = q_order8; break;
        default:      filter->n_sections = 0; return;
    }
    float f = cut_frec / sample_frec;
    filter->n_sections = order / 2;
    for(uint8_t i = 0; i < filter->n_sections; i++){
        gen(filter->coeffs[i], f, q[i]);
    }
    memset(filter->delay, 0, sizeof(filter->delay));
}

/*==================[external functions definition]==========================*/

void IirLowPassInit(iir_filter_t *filter, float sample_frec, float cut_frec, filter_order_t order){
    IirInit(filter, dsps_biquad_gen_lpf_f32, sample_frec, cut_frec, order);
}

void IirHiPassInit(iir_filter_t *filter, float sample_frec, float cut_frec, filter_order_t order){
    IirInit(filter, dsps_biquad_gen_hpf_f32, sample_frec, cut_frec, order);
}

void IirFilter(iir_filter_t *filter, float *input_signal, float *output_signal, int16_t signal_lenght){
    // the first section reads the input; the next ones filter the output in place
    float *input = input_signal;
    for(uint8_t i = 0; i < filter->n_sections; i++){
        dsps_biquad_f32(input, output_signal, signal_lenght, filter->coeffs[i], filter->delay[i]);
        input = output_signal;
    }
}

void LowPassInit(float sample_frec, float cut_frec, filter_order_t order){
    IirLowPassInit(&lp_filter, sample_frec, cut_frec, order);
}

void HiPassInit(float sample_frec, float cut_frec, filter_order_t order){
    IirHiPassInit(&hp_filter, sample_frec, cut_frec, order);
}

void LowPassFilter(float * input_signal, float * output_signal, int16_t signal_lenght){
    IirFilter(&lp_filter, input_signal, output_signal, signal_lenght);
}

void HiPassFilter(float * input_signal, float * output_signal, int16_t signal_lenght){
    IirFilter(&hp_filter, input_signal, output_signal, signal_lenght);
}

/*==================[end of file]============================================*/
