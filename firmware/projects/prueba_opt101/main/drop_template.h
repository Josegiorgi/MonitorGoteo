#ifndef DROP_TEMPLATE_H_
#define DROP_TEMPLATE_H_
/** @file drop_template.h
 *
 * Plantilla de la gota, para el detector por correlación cruzada (xcorr_detector).
 *
 * Es el promedio de las 35 gotas de FiltradaconGota.csv, alineadas en el valle más profundo, en una
 * ventana de -15ms a +30ms alrededor de él. Cada gota se normaliza a energía 1 antes de promediar, para que
 * las gotas rápidas (grandes) no dominen sobre las lentas (chicas). Se generó con
 * ../../analisis_opt101/analizar_correlacion.py, que también calcula el umbral (XCORR_THRESHOLD).
 * El valor está en unidades arbitrarias (el detector la vuelve a normalizar), sobre la señal DESPUÉS del
 * pasabanda (MODE_FILTERED).
 *
 * @note La forma depende de fs, del filtro y del montaje del sensor: esta plantilla vale para
 * SAMPLE_PERIOD_US = 700 (fs = 1428.6Hz), el pasabanda de prueba_opt101.c (pasaaltos 60Hz + pasabajos
 * 300Hz, ORDER_4, sin notch) y el OPT101 con Rf = 100k, C = 3.9nF, antialias 4k7 + 100nF y LED con 1k2.
 * Si cambia alguno de esos parámetros hay que volver a generarla con el script de análisis.
 */

#define DROP_TEMPLATE_LEN 63

static const float DROP_TEMPLATE[DROP_TEMPLATE_LEN] = {
    0.00003f, -0.00032f, -0.00093f, -0.00194f, -0.00601f, -0.01748f,
    -0.03615f, -0.05007f, -0.03909f, 0.00537f, 0.05719f, 0.06381f,
    0.00087f, -0.08270f, -0.09589f, 0.01784f, 0.23259f, 0.42004f,
    0.38835f, 0.06373f, -0.34757f, -0.52050f, -0.35761f, -0.05918f,
    0.13284f, 0.14893f, 0.07046f, -0.00394f, -0.03772f, -0.04380f,
    -0.04063f, -0.03323f, -0.02072f, -0.00459f, 0.01161f, 0.02483f,
    0.03369f, 0.03840f, 0.03962f, 0.03807f, 0.03475f, 0.03002f,
    0.02335f, 0.01505f, 0.00679f, -0.00012f, -0.00563f, -0.00994f,
    -0.01286f, -0.01420f, -0.01432f, -0.01384f, -0.01297f, -0.01158f,
    -0.00953f, -0.00724f, -0.00545f, -0.00404f, -0.00209f, 0.00033f,
    0.00207f, 0.00267f, 0.00274f,
};

#endif /* DROP_TEMPLATE_H_ */
