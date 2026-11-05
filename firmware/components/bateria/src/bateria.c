/*==================[inclusions]=============================================*/
#include "bateria.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
typedef struct {
    uint16_t mv;    /*!< Tensión de la batería, en mV */
    uint8_t pct;    /*!< Carga que le corresponde, en % */
} punto_t;

/*==================[internal data definition]===============================*/
/* Curva de descarga típica de una celda Li-ion/Li-Po, sin carga (en reposo). Entre dos puntos se
 * interpola en línea recta. Con la batería trabajando la tensión cae un poco, así que el porcentaje
 * real puede ser algo mayor. Reemplazar por la curva medida del equipo cuando se mida la autonomía. */
static const punto_t curva[] = {
    {3270, 0},  {3610, 5},  {3690, 10}, {3710, 15}, {3730, 20}, {3750, 25}, {3770, 30},
    {3790, 35}, {3800, 40}, {3820, 45}, {3840, 50}, {3850, 55}, {3870, 60}, {3910, 65},
    {3950, 70}, {3980, 75}, {4020, 80}, {4080, 85}, {4110, 90}, {4150, 95}, {4200, 100},
};
#define CURVA_PUNTOS (sizeof(curva) / sizeof(curva[0]))

static uint16_t muestras_mv[BATERIA_MUESTRAS];   /*!< Últimas lecturas en el pin del ADC */
static uint8_t cantidad = 0;
static uint8_t siguiente = 0;
static int64_t ultima_lectura_us = 0;
static bool iniciada = false;

/*==================[external functions definition]==========================*/
bool Bateria_Init(void) {
    analog_input_config_t config = {
        .input = BATERIA_CANAL_ADC,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&config);
    cantidad = 0;
    siguiente = 0;
    ultima_lectura_us = 0;
    iniciada = true;
    return true;
}

void Bateria_Muestrear(bool omitir) {
    if (!iniciada || omitir) {
        return;
    }
    int64_t ahora = esp_timer_get_time();
    if (ultima_lectura_us != 0 && (ahora - ultima_lectura_us) < (int64_t)BATERIA_PERIODO_MUESTRA_MS * 1000) {
        return;
    }
    ultima_lectura_us = ahora;

    uint16_t mv = 0;
    AnalogInputReadSingle(BATERIA_CANAL_ADC, &mv);
    muestras_mv[siguiente] = mv;
    siguiente = (siguiente + 1) % BATERIA_MUESTRAS;
    if (cantidad < BATERIA_MUESTRAS) {
        cantidad++;
    }
}

uint16_t Bateria_PinMv(void) {
    if (cantidad == 0) {
        return 0;
    }
    uint32_t suma = 0;
    for (uint8_t i = 0; i < cantidad; i++) {
        suma += muestras_mv[i];
    }
    return (uint16_t)((suma + cantidad / 2) / cantidad);
}

uint16_t Bateria_Mv(void) {
    uint64_t mv = (uint64_t)Bateria_PinMv() * (BATERIA_R_ARRIBA_OHM + BATERIA_R_ABAJO_OHM) * BATERIA_AJUSTE_PERMIL;
    mv = (mv + (uint64_t)BATERIA_R_ABAJO_OHM * 500) / ((uint64_t)BATERIA_R_ABAJO_OHM * 1000);
    return (mv > UINT16_MAX) ? UINT16_MAX : (uint16_t)mv;
}

bool Bateria_Valida(void) {
    return cantidad >= BATERIA_MUESTRAS / 2 && Bateria_Mv() >= BATERIA_MV_MINIMO_VALIDO;
}

uint8_t Bateria_PctDeMv(uint16_t mv) {
    if (mv <= curva[0].mv) {
        return 0;
    }
    if (mv >= curva[CURVA_PUNTOS - 1].mv) {
        return 100;
    }
    for (uint8_t i = 1; i < CURVA_PUNTOS; i++) {
        if (mv <= curva[i].mv) {
            uint32_t ancho = curva[i].mv - curva[i - 1].mv;
            uint32_t alto = curva[i].pct - curva[i - 1].pct;
            uint32_t sobre = mv - curva[i - 1].mv;
            return (uint8_t)(curva[i - 1].pct + (alto * sobre + ancho / 2) / ancho);   // redondeado
        }
    }
    return 100;
}

uint8_t Bateria_Pct(void) {
    return Bateria_PctDeMv(Bateria_Mv());
}
