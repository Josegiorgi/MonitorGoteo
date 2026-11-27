/*==================[inclusions]=============================================*/
#include "autotest.h"
#include "estado.h"
#include "ui.h"
#include "buzzer_alarm.h"
#include "optical_sensor.h"
#include "inclinacion.h"
#include "bateria.h"
#include "registro.h"
#include "esp_log.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define DURACION_INICIO_MS    5000  /*!< Duración mínima de la pantalla de inicio (autotest) */
#define PERIODO_CHEQUEO_MS    1000  /*!< Cada cuánto se hace o reintenta un chequeo del autotest */
#define PERIODO_ANIMACION_MS  42    /*!< Cada cuánto avanza un cuadro de la animación del inicio */
#define BASE_OPTICA_MIN_MV    50      /*!< Nivel base del fotodiodo por debajo de esto: en cero (sensor sin señal). Provisorio: se fija con ensayos de banco */
#define BASE_OPTICA_MAX_MV    2400    /*!< Por encima de esto: saturado (el ADC con 12 dB llega a ~2500 mV). Provisorio */
#define BATERIA_MIN_ARRANQUE  10      /*!< El autotest pide más que este % de batería (o cargador, que sube la tensión) */

static const char *TAG = "autotest";

/*==================[internal data definition]===============================*/
/* Chequeos del autotest, en el orden de la especificación. Si uno falla se muestra "Falla: <nombre>" y la acción */
static ui_chequeo_t chequeos[UI_CHEQUEOS_MAX] = {
    {"Pantalla", UI_CHEQUEO_PENDIENTE, NULL},
    {"Acelerometro", UI_CHEQUEO_PENDIENTE, NULL},
    {"Sensor optico", UI_CHEQUEO_PENDIENTE, NULL},
    {"Memoria", UI_CHEQUEO_PENDIENTE, NULL},
    {"Bateria baja", UI_CHEQUEO_PENDIENTE, "Conectar cargador"},
};
static bool oled_ok = false;
static bool sensor_ok = false;
static bool acel_ok = false;
static bool pitando_falla = false;     /*!< true mientras el buzzer avisa que la pantalla falló en el autotest */

/*==================[internal functions definition]==========================*/
/* Chequeos del autotest. Cada uno devuelve true si el componente responde. */
static bool ChequearOled(void) {
    return oled_ok;
}

static bool ChequearAcelerometro(void) {
    if (!acel_ok) {
        acel_ok = Inclinacion_Init();   // el autotest reintenta cada segundo: por si el chip tardó en responder
    }
    return acel_ok && Inclinacion_Valida(NULL);
}

/** El nivel base de los dos fotodiodos (luz recibida sin gota) no está en cero ni saturado. Vale con o sin cámara colocada */
static bool ChequearSensorOptico(void) {
    if (!sensor_ok) {
        return false;
    }
    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        float base = OpticalSensor_BaseMv(ch);
        ESP_LOGI(TAG, "sensor optico: nivel base del canal %d = %.0f mV (rango %d a %d mV)", ch + 1, (double)base,
                 BASE_OPTICA_MIN_MV, BASE_OPTICA_MAX_MV);
        if (base < BASE_OPTICA_MIN_MV || base > BASE_OPTICA_MAX_MV) {
            return false;
        }
    }
    return true;
}

/** La memoria no volátil (NVS) se puede iniciar, escribir y leer */
static bool ChequearMemoria(void) {
    return Registro_Probar();
}

/** Batería con más de BATERIA_MIN_ARRANQUE % (o cargador conectado: sube la tensión). Sin batería conectada (solo USB) pasa */
static bool ChequearBateria(void) {
    return !Bateria_Valida() || Bateria_Pct() > BATERIA_MIN_ARRANQUE;
}

static bool (*const funciones_chequeo[UI_CHEQUEOS_MAX])(void) = {
    ChequearOled, ChequearAcelerometro, ChequearSensorOptico, ChequearMemoria, ChequearBateria,
};

static bool TodosOk(void) {
    for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
        if (chequeos[i].estado != UI_CHEQUEO_OK) {
            return false;
        }
    }
    return true;
}

/*==================[external functions definition]==========================*/
void Autotest_Init(bool oled, bool sensor, bool acel) {
    oled_ok = oled;
    sensor_ok = sensor;
    acel_ok = acel;
    UI_SetChequeos(chequeos, UI_CHEQUEOS_MAX);
}

void Autotest_Ejecutar(bool bloquear) {
    uint32_t proximo_ms[UI_CHEQUEOS_MAX];
    uint32_t t_ms;
    int64_t inicio_us = esp_timer_get_time();

    for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
        proximo_ms[i] = i * PERIODO_CHEQUEO_MS + 500;   // se escalonan; el de batería espera a tener lecturas
    }

    do {
        Bateria_Muestrear(false);   // la carga se mide mientras se hace el autotest
        t_ms = (esp_timer_get_time() - inicio_us) / 1000;
        for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
            if (chequeos[i].estado != UI_CHEQUEO_OK && t_ms >= proximo_ms[i]) {
                chequeos[i].estado = funciones_chequeo[i]() ? UI_CHEQUEO_OK : UI_CHEQUEO_FALLA;
                proximo_ms[i] = t_ms + PERIODO_CHEQUEO_MS;
            }
        }
        // Si la pantalla falla no hay cómo mostrar un texto: avisa con el buzzer hasta que responda
        if (chequeos[0].estado == UI_CHEQUEO_FALLA && !pitando_falla) {
            pitando_falla = true;
            BuzzerAlarm_Start(BUZZER_ALARM_FLUJO_DETENIDO);
        } else if (chequeos[0].estado == UI_CHEQUEO_OK && pitando_falla) {
            pitando_falla = false;
            BuzzerAlarm_Stop();
        }
        if (bloquear && t_ms >= DURACION_INICIO_MS && !TodosOk()) {
            UI_Ir(UI_PANTALLA_AUTOTEST, &datos);   // un chequeo falla: se muestra cuál (se reintenta cada segundo)
        } else {
            UI_PantallaInicio(t_ms / PERIODO_ANIMACION_MS);
        }
        // Normal: no deja continuar con una falla. Con un registro pendiente (reinicio) no hay enfermera para leerla: sigue y suena la alarma
    } while (t_ms < DURACION_INICIO_MS || (bloquear && !TodosOk()));
    if (pitando_falla) {
        pitando_falla = false;
        BuzzerAlarm_Stop();
    }
}
