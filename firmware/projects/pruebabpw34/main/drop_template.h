#ifndef DROP_TEMPLATE_H_
#define DROP_TEMPLATE_H_
/** @file drop_template.h
 *
 * Plantillas de la gota, una por canal, para el detector por correlación cruzada (xcorr_detector).
 *
 * Cada plantilla es el promedio de las gotas de más de 400 mV pico a pico en ese canal de Filtrada2.csv
 * (dispositivo fijo) y Filtrada.csv (dispositivo movido): 101 gotas en el canal 1 y 126 en el canal 2,
 * alineadas en el valle más profundo, en una ventana de -15ms a +30ms alrededor de él. Cada gota se
 * normaliza a energía 1 antes de promediar. Se generaron con
 * ../../analisis_bpw34/analizar_correlacion_bpw34.py, que también calcula los umbrales
 * (XCORR_THRESHOLD_CH1 / XCORR_THRESHOLD_CH2). Los valores están en unidades arbitrarias (el detector
 * las vuelve a normalizar), sobre la señal DESPUÉS del pasabanda (MODE_FILTERED).
 * Hay una plantilla por canal porque cada fotodiodo ve la gota algo distinta (aunque se parecen mucho).
 *
 * @note La forma depende de fs, del filtro digital y del antialias: estas plantillas valen para
 * SAMPLE_PERIOD_US = 1200 (fs ≈ 833 Hz), el pasabanda de pruebabpw34.c (pasaaltos 20 Hz +
 * pasabajos 260 Hz, ORDER_4) y el antialias RC de 4.7 kΩ + 100 nF. Si cambia alguno de esos
 * parámetros hay que volver a generarlas con el script de análisis.
 */

#define DROP_TEMPLATE_LEN 37

/* BPW34 n°1 (GPIO_2) */
static const float DROP_TEMPLATE_CH1[DROP_TEMPLATE_LEN] = {
    0.00108f, -0.00015f, 0.00122f, 0.00431f, 0.00174f, -0.02365f,
    -0.09541f, -0.16877f, -0.05172f, 0.28565f, 0.32407f, -0.22247f,
    -0.57179f, -0.16072f, 0.30878f, 0.22537f, 0.00746f, 0.11127f,
    0.28102f, 0.21606f, 0.09240f, 0.06002f, 0.02463f, -0.02934f,
    -0.05369f, -0.06814f, -0.08566f, -0.09365f, -0.09583f, -0.09569f,
    -0.08996f, -0.08181f, -0.07433f, -0.06488f, -0.05362f, -0.04371f,
    -0.03404f,
};

/* BPW34 n°2 (GPIO_3) */
static const float DROP_TEMPLATE_CH2[DROP_TEMPLATE_LEN] = {
    -0.00124f, -0.00034f, 0.00093f, 0.00230f, 0.00193f, -0.01731f,
    -0.08559f, -0.16945f, -0.07104f, 0.27855f, 0.36161f, -0.18379f,
    -0.58472f, -0.18767f, 0.29343f, 0.19591f, -0.00566f, 0.13130f,
    0.28233f, 0.19808f, 0.08918f, 0.06152f, 0.02204f, -0.02618f,
    -0.04732f, -0.06475f, -0.08299f, -0.08952f, -0.09070f, -0.09127f,
    -0.08749f, -0.08014f, -0.07104f, -0.06091f, -0.05080f, -0.04009f,
    -0.02893f,
};

#endif /* DROP_TEMPLATE_H_ */
