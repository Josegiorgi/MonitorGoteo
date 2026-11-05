/*==================[inclusions]=============================================*/
#include "buzzer_alarm.h"
#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/
#define TAREA_STACK     2048
#define TAREA_PRIORIDAD 5
#define PASO_MAX_MS     50     /*!< Los pasos se esperan de a este tiempo para poder cortar un patrón a mitad */

/** Un paso del patrón: tono durante duracion_ms (frecuencia 0 = silencio). Termina con duracion_ms = 0 */
typedef struct {
    uint16_t frecuencia_hz;
    uint16_t duracion_ms;
} paso_t;

/*==================[internal data definition]===============================*/
/* Todos los tonos están entre 3300 y 4200 Hz: según la curva de la hoja de datos del buzzer
 * (CPT-2016-75-SMT-TR) es donde más suena (~83 dB entre 3,4 y 4,2 kHz; a 1 kHz y a 2 kHz queda
 * 13 a 17 dB más abajo). Como la banda es angosta, los patrones se distinguen sobre todo por el
 * ritmo y la cantidad de pitidos, y poco por el tono.
 * Es una decisión de prototipo de laboratorio, para que las alarmas se escuchen bien. Una versión
 * más madura debería ajustarse a IEC 60601-1-8 e IEC 60601-2-24 (nivel sonoro mínimo y
 * características de las señales sonoras), y medir el nivel a 1 m. */
static const paso_t p_volumen_completo[] = {
    {3600, 120}, {4200, 120}, {3600, 120}, {4200, 120}, {3600, 120}, {4200, 120}, {0, 700}, {0, 0}};
static const paso_t p_flujo_detenido[] = {
    {4000, 100}, {0, 100}, {4000, 100}, {0, 100}, {4000, 100}, {0, 900}, {0, 0}};
static const paso_t p_fin_sachet[] = {
    {3400, 200}, {3800, 200}, {4200, 300}, {0, 1500}, {0, 0}};
static const paso_t p_colocacion[] = {
    {3500, 150}, {0, 150}, {3500, 150}, {0, 1200}, {0, 0}};
static const paso_t p_falla_equipo[] = {
    {4200, 100}, {3300, 100}, {4200, 100}, {3300, 100}, {4200, 100}, {3300, 100}, {0, 800}, {0, 0}};
static const paso_t p_bateria_critica[] = {
    {3300, 500}, {0, 200}, {3300, 500}, {0, 1000}, {0, 0}};
static const paso_t p_desviacion_fuerte[] = {
    {3400, 250}, {4200, 250}, {3400, 250}, {4200, 250}, {0, 900}, {0, 0}};
static const paso_t p_goteo_sin_monitoreo[] = {
    {3800, 400}, {0, 300}, {3800, 400}, {0, 1400}, {0, 0}};
static const paso_t p_bateria_baja[] = {
    {3300, 700}, {0, 1500}, {0, 0}};

/* Sonidos de la interfaz: también entre 3300 y 4200 Hz, que es donde el buzzer suena más */
static const paso_t p_encendido[] = {{3300, 110}, {3700, 110}, {4200, 220}, {0, 0}};
static const paso_t p_apagado[] = {{4200, 110}, {3700, 110}, {3300, 220}, {0, 0}};
static const paso_t p_click[] = {{3800, 50}, {0, 0}};
static const paso_t p_giro[] = {{3400, 12}, {0, 0}};

#define TOTAL_PATRONES (BUZZER_ALARM_CANTIDAD + BUZZER_EFECTO_CANTIDAD)
static const paso_t *const patrones[TOTAL_PATRONES] = {
    [BUZZER_ALARM_VOLUMEN_COMPLETO]  = p_volumen_completo,
    [BUZZER_ALARM_FLUJO_DETENIDO]    = p_flujo_detenido,
    [BUZZER_ALARM_FIN_SACHET]        = p_fin_sachet,
    [BUZZER_ALARM_COLOCACION]        = p_colocacion,
    [BUZZER_ALARM_FALLA_EQUIPO]      = p_falla_equipo,
    [BUZZER_ALARM_BATERIA_CRITICA]   = p_bateria_critica,
    [BUZZER_ALARM_DESVIACION_FUERTE] = p_desviacion_fuerte,
    [BUZZER_ALARM_GOTEO_SIN_MONITOREO] = p_goteo_sin_monitoreo,
    [BUZZER_ALARM_BATERIA_BAJA]      = p_bateria_baja,
    [BUZZER_ALARM_CANTIDAD + BUZZER_EFECTO_ENCENDIDO] = p_encendido,
    [BUZZER_ALARM_CANTIDAD + BUZZER_EFECTO_APAGADO]   = p_apagado,
    [BUZZER_ALARM_CANTIDAD + BUZZER_EFECTO_CLICK]     = p_click,
    [BUZZER_ALARM_CANTIDAD + BUZZER_EFECTO_GIRO]      = p_giro,
};

static const char *const nombres[BUZZER_ALARM_CANTIDAD] = {
    "VOLUMEN_COMPLETO", "FLUJO_DETENIDO", "FIN_SACHET", "COLOCACION", "FALLA_EQUIPO",
    "BATERIA_CRITICA", "DESVIACION_FUERTE", "GOTEO_SIN_MONITOREO", "BATERIA_BAJA"};

static TaskHandle_t tarea = NULL;
static volatile bool tono_activo = false;  /*!< true mientras el buzzer emite un tono */
static volatile int patron_actual = -1;   /*!< -1 = silencio */
static volatile uint32_t version = 0;     /*!< Cambia con cada Start/Stop: la tarea corta el patrón que estaba sonando */

/*==================[internal functions definition]==========================*/
/** Espera ms milisegundos o hasta que cambie la versión. true si se completó la espera. */
static bool Esperar(uint16_t ms, uint32_t ver) {
    while (ms > 0) {
        uint16_t t = (ms > PASO_MAX_MS) ? PASO_MAX_MS : ms;
        vTaskDelay(pdMS_TO_TICKS(t));
        ms -= t;
        if (version != ver) {
            return false;
        }
    }
    return true;
}

static void TareaPatrones(void *arg) {
    while (1) {
        int p = patron_actual;
        uint32_t ver = version;
        if (p < 0) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // sin patrón: duerme hasta el próximo Start
            continue;
        }
        for (const paso_t *s = patrones[p]; s->duracion_ms > 0; s++) {
            if (s->frecuencia_hz > 0) {
                BuzzerSetFrec(s->frecuencia_hz);
                BuzzerOn();
                tono_activo = true;
            } else {
                BuzzerOff();
                tono_activo = false;
            }
            if (!Esperar(s->duracion_ms, ver)) {
                break;
            }
        }
        BuzzerOff();
        tono_activo = false;
        if (p >= BUZZER_ALARM_CANTIDAD && version == ver) {
            patron_actual = -1;   // un sonido de la interfaz suena una sola vez; las alarmas se repiten
        }
    }
}

/*==================[external functions definition]==========================*/
bool BuzzerAlarm_Init(gpio_t pin) {
    if (tarea != NULL) {
        return true;
    }
    BuzzerInit(pin);
    return xTaskCreate(TareaPatrones, "alarma", TAREA_STACK, NULL, TAREA_PRIORIDAD, &tarea) == pdPASS;
}

void BuzzerAlarm_Start(buzzer_alarm_t patron) {
    if (tarea == NULL || patron >= BUZZER_ALARM_CANTIDAD) {
        return;
    }
    patron_actual = patron;
    version++;
    xTaskNotifyGive(tarea);
}

void BuzzerAlarm_Stop(void) {
    if (tarea == NULL) {
        return;
    }
    patron_actual = -1;
    version++;
    xTaskNotifyGive(tarea);
    BuzzerOff();
    tono_activo = false;
}

bool BuzzerAlarm_Sonando(void) {
    return tono_activo;
}

void BuzzerAlarm_Efecto(buzzer_efecto_t efecto) {
    if (tarea == NULL || efecto >= BUZZER_EFECTO_CANTIDAD) {
        return;
    }
    if (patron_actual >= 0 && patron_actual < BUZZER_ALARM_CANTIDAD) {
        return;   // hay una alarma sonando: no se la tapa
    }
    patron_actual = BUZZER_ALARM_CANTIDAD + efecto;
    version++;
    xTaskNotifyGive(tarea);
}

void BuzzerAlarm_EsperarFin(uint32_t max_ms) {
    while (max_ms > 0 && patron_actual >= BUZZER_ALARM_CANTIDAD) {
        vTaskDelay(pdMS_TO_TICKS(10));
        max_ms = (max_ms > 10) ? max_ms - 10 : 0;
    }
}

const char *BuzzerAlarm_Nombre(buzzer_alarm_t patron) {
    return (patron < BUZZER_ALARM_CANTIDAD) ? nombres[patron] : "?";
}
