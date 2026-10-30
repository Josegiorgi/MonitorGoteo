/*==================[inclusions]=============================================*/
#include "drop_detection.h"
#include "optical_sensor.h"
#include "drop_template.h"
#include "xcorr_detector.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define XCORR_THRESHOLD_CH1  160.0f   /*!< Umbrales y tiempos de pruebabpw34 (ver ahí cómo salen del análisis) */
#define XCORR_THRESHOLD_CH2  140.0f
#define XCORR_PEAK_WINDOW_MS 15
#define XCORR_REFRACTORY_MS  100
#define MS_TO_SAMPLES(ms)    (((ms) * 1000UL) / OPTICAL_SENSOR_SAMPLE_PERIOD_US)

/*==================[internal data definition]===============================*/
static xcorr_detector_t detector[OPTICAL_SENSOR_CHANNELS];
static uint32_t merge_refractory_left = 0;   /*!< Muestras que faltan para volver a avisar una gota */
static drop_cb_t drop_cb = NULL;
static volatile float ultimo_pico = 0.0f;    /*!< Pico de la correlación de la última gota detectada, en mV */
static volatile int ultimo_canal = 0;        /*!< Canal (1 o 2) que detectó la última gota */
static const float *const drop_templates[OPTICAL_SENSOR_CHANNELS] = {DROP_TEMPLATE_CH1, DROP_TEMPLATE_CH2};
static const float xcorr_thresholds[OPTICAL_SENSOR_CHANNELS] = {XCORR_THRESHOLD_CH1, XCORR_THRESHOLD_CH2};

/*==================[internal functions definition]==========================*/
/** Se llama con cada muestra del sensor: cada canal detecta por su cuenta y una gota se avisa una sola vez. */
static void OnSample(const uint16_t raw_mV[OPTICAL_SENSOR_CHANNELS], const float filtered_mV[OPTICAL_SENSOR_CHANNELS]) {
    bool drop_detected = false;
    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        bool channel_detected = XCorrDetectorProcess(&detector[ch], filtered_mV[ch], NULL);
        if (channel_detected && merge_refractory_left == 0) {
            drop_detected = true;
            ultimo_pico = XCorrDetectorGetLastPeak(&detector[ch]);
            ultimo_canal = ch + 1;
            merge_refractory_left = MS_TO_SAMPLES(XCORR_REFRACTORY_MS);
        }
    }
    if (merge_refractory_left > 0 && !drop_detected) {
        merge_refractory_left--;
    }
    if (drop_detected && drop_cb != NULL) {
        drop_cb(esp_timer_get_time());
    }
}

/*==================[external functions definition]==========================*/
float DropDetection_UltimoPico(int *canal) {
    if (canal != NULL) {
        *canal = ultimo_canal;
    }
    return ultimo_pico;
}

bool DropDetection_Init(drop_cb_t cb) {
    drop_cb = cb;
    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        xcorr_config_t xcorr_config = {
            .template_signal = drop_templates[ch],
            .template_len = DROP_TEMPLATE_LEN,
            .threshold = xcorr_thresholds[ch],
            .peak_window = MS_TO_SAMPLES(XCORR_PEAK_WINDOW_MS),
            .refractory = MS_TO_SAMPLES(XCORR_REFRACTORY_MS),
        };
        if (!XCorrDetectorInit(&detector[ch], &xcorr_config)) {
            return false;
        }
    }
    return OpticalSensor_Init(OnSample);
}
