/*==================[inclusions]=============================================*/
#include "optical_sensor.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
#include "iir_filter.h"

#ifdef CONFIG_OPTICAL_SENSOR_OPT101
#error "El OPT101 todavia no esta migrado a este componente: usar projects/prueba_opt101 o elegir BPW34 en menuconfig"
#endif

/*==================[macros and definitions]=================================*/
#define ADC_CHANNEL_BPW34_1  CH2      /*!< Salida del amplificador del BPW34 n°1 (GPIO_2) */
#define ADC_CHANNEL_BPW34_2  CH3      /*!< Salida del amplificador del BPW34 n°2 (GPIO_3) */
#define CUTOFF_HIGHPASS_HZ   20.0f    /*!< Mismos cortes y orden que pruebabpw34 (ver ahí el análisis) */
#define CUTOFF_LOWPASS_HZ    260.0f
#define FILTER_ORDER         ORDER_4
#define BASELINE_SAMPLES     100
#define TASK_STACK           4096
#define TASK_PRIORITY        5

/*==================[internal data definition]===============================*/
static const adc_ch_t adc_channels[OPTICAL_SENSOR_CHANNELS] = {ADC_CHANNEL_BPW34_1, ADC_CHANNEL_BPW34_2};
static iir_filter_t lowpass[OPTICAL_SENSOR_CHANNELS];
static iir_filter_t highpass[OPTICAL_SENSOR_CHANNELS];
static float baseline_mV[OPTICAL_SENSOR_CHANNELS];
static TaskHandle_t task_handle = NULL;
static optical_sensor_cb_t sample_cb = NULL;

/*==================[internal functions definition]==========================*/
/** Interrupción del timer: solo avisa a la tarea de muestreo. */
static void FuncTimerSample(void *param) {
    vTaskNotifyGiveFromISR(task_handle, pdFALSE);
}

/** Arma el pasabanda de cada canal y mide su nivel base promediando BASELINE_SAMPLES lecturas. */
static void BandPassInit(void) {
    float fs = 1000000.0f / OPTICAL_SENSOR_SAMPLE_PERIOD_US;
    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        IirLowPassInit(&lowpass[ch], fs, CUTOFF_LOWPASS_HZ, FILTER_ORDER);
        IirHiPassInit(&highpass[ch], fs, CUTOFF_HIGHPASS_HZ, FILTER_ORDER);
        float sum = 0;
        for (int i = 0; i < BASELINE_SAMPLES; i++) {
            uint16_t value_mV;
            AnalogInputReadSingle(adc_channels[ch], &value_mV);
            sum += value_mV;
        }
        baseline_mV[ch] = sum / BASELINE_SAMPLES;
    }
}

/** Filtra una muestra de un canal: resta el nivel base (para no cargar los biquads con una
 * continua de ~1 V, ver el comentario de BandPassFilter en pruebabpw34), pasabajos y pasaaltos. */
static float BandPassFilter(int ch, float value_mV) {
    float x = value_mV - baseline_mV[ch];
    IirFilter(&lowpass[ch], &x, &x, 1);
    IirFilter(&highpass[ch], &x, &x, 1);
    return x;
}

static void SamplingTask(void *pvParameter) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // espera al timer
        uint16_t raw_mV[OPTICAL_SENSOR_CHANNELS];
        float filtered_mV[OPTICAL_SENSOR_CHANNELS];
        for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
            AnalogInputReadSingle(adc_channels[ch], &raw_mV[ch]);
            filtered_mV[ch] = BandPassFilter(ch, (float)raw_mV[ch]);
        }
        if (sample_cb != NULL) {
            sample_cb(raw_mV, filtered_mV);
        }
    }
}

/*==================[external functions definition]==========================*/
bool OpticalSensor_Init(optical_sensor_cb_t cb) {
    if (task_handle != NULL) {
        return true;
    }
    sample_cb = cb;

    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        analog_input_config_t adc_config = {
            .input = adc_channels[ch],
            .mode = ADC_SINGLE,
            .func_p = NULL,
            .param_p = NULL,
        };
        AnalogInputInit(&adc_config);
    }
    BandPassInit();

    if (xTaskCreate(&SamplingTask, "OPTICAL", TASK_STACK, NULL, TASK_PRIORITY, &task_handle) != pdPASS) {
        return false;
    }
    timer_config_t sample_timer = {
        .timer = TIMER_A,
        .period = OPTICAL_SENSOR_SAMPLE_PERIOD_US,
        .func_p = FuncTimerSample,
        .param_p = NULL,
    };
    TimerInit(&sample_timer);
    TimerStart(TIMER_A);
    return true;
}

float OpticalSensor_BaseMv(int canal) {
    return (canal >= 0 && canal < OPTICAL_SENSOR_CHANNELS) ? baseline_mV[canal] : 0.0f;
}
