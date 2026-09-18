#ifndef DROP_TEMPLATE_H_
#define DROP_TEMPLATE_H_
/** @file drop_template.h
 *
 * Plantilla de la gota, para el detector por correlación cruzada (xcorr_detector).
 *
 * Es el promedio de las 8 gotas de FiltradaCONGOTA.csv, en una ventana de +-20ms alrededor de cada
 * pico (ver ../../analisis_opt101, sección "Correlación cruzada con la plantilla"). El valor está
 * en mV de la señal DESPUÉS del pasabanda (MODE_FILTERED).
 *
 * @note La forma depende de fs y del filtro: esta plantilla vale para SAMPLE_PERIOD_US = 700
 * (fs = 1428.6Hz) y el pasabanda de prueba_opt101.c (pasaaltos 85Hz + pasabajos ~195-234Hz, ORDER_4).
 * Si cambia alguno de esos parámetros hay que volver a generarla con el script de análisis.
 */

#define DROP_TEMPLATE_LEN 56

static const float DROP_TEMPLATE[DROP_TEMPLATE_LEN] = {
    0.49875f, 0.33500f, 0.09875f, -0.08875f, -0.16250f, -0.14000f,
    -0.09625f, -0.08875f, -0.10875f, -0.14750f, -0.18750f, -0.16750f,
    -0.05375f, 0.11000f, 0.22875f, 0.26000f, 0.28125f, 0.57250f,
    1.45750f, 2.55875f, 2.58375f, 0.11125f, -5.28625f, -11.86125f,
    -14.72750f, -8.46125f, 6.74625f, 22.48500f, 27.98375f, 18.95375f,
    0.72875f, -16.36000f, -24.14625f, -20.90125f, -10.70500f, 0.29125f,
    7.57500f, 9.87750f, 8.46875f, 5.52125f, 2.76000f, 0.90875f,
    -0.10625f, -0.67250f, -1.09375f, -1.47750f, -1.75000f, -1.79250f,
    -1.58000f, -1.14750f, -0.60125f, -0.08500f, 0.29375f, 0.53125f,
    0.68000f, 0.75750f,
};

#endif /* DROP_TEMPLATE_H_ */
