#ifndef DROP_TEMPLATE_H_
#define DROP_TEMPLATE_H_
/** @file drop_template.h
 *
 * Plantillas de la gota, una por canal, para el detector por correlación cruzada (xcorr_detector).
 *
 * Cada plantilla es el promedio de las 7 gotas de SeñalFiltradaconGoteo.csv, en una ventana de
 * ±20 ms alrededor del mínimo de cada gota (ver ../../analisis_bpw34, sección "Exportar las
 * plantillas al firmware"). El valor está en mV de la señal DESPUÉS del pasabanda (MODE_FILTERED).
 * Hay una plantilla por canal porque la forma de la gota no es igual en los dos fotodiodos.
 *
 * @note La forma depende de fs, del filtro digital y del antialias: estas plantillas valen para
 * SAMPLE_PERIOD_US = 1200 (fs ≈ 833 Hz), el pasabanda de pruebabpw34.c (pasaaltos 5 Hz +
 * pasabajos 260 Hz, ORDER_4) y el antialias RC de 4.7 kΩ + 100 nF. Si cambia alguno de esos
 * parámetros hay que volver a generarlas con el script de análisis.
 */

#define DROP_TEMPLATE_LEN 32

/* BPW34 n°1 (GPIO_2) */
static const float DROP_TEMPLATE_CH1[DROP_TEMPLATE_LEN] = {
    -1.28571f, -3.28571f, -1.85714f, 0.85714f, 1.14286f, -1.14286f,
    -2.00000f, 1.85714f, 6.28571f, 6.42857f, 2.42857f, -2.28571f,
    -4.00000f, -24.71429f, -127.28571f, -309.57143f, -392.57143f, -283.57143f,
    -213.28571f, -247.00000f, -101.14286f, 179.14286f, 251.71429f, 164.57143f,
    148.57143f, 165.57143f, 143.57143f, 130.71429f, 132.28571f, 120.14286f,
    106.71429f, 101.85714f,
};

/* BPW34 n°2 (GPIO_3) */
static const float DROP_TEMPLATE_CH2[DROP_TEMPLATE_LEN] = {
    -0.42857f, 1.85714f, -0.85714f, -1.14286f, 4.71429f, 7.00000f,
    0.57143f, -4.42857f, -2.42857f, -11.28571f, -83.57143f, -225.85714f,
    -215.42857f, 100.71429f, 233.00000f, -182.85714f, -469.14286f, -104.42857f,
    306.28571f, 176.85714f, -130.00000f, -91.85714f, 107.85714f, 126.85714f,
    70.42857f, 78.42857f, 82.71429f, 64.00000f, 61.71429f, 60.57143f,
    52.14286f, 51.00000f,
};

#endif /* DROP_TEMPLATE_H_ */
