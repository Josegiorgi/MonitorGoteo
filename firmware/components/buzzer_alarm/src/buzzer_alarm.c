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
static const paso_t p_volumen_completo[] = {
    {2400, 120}, {3000, 120}, {2400, 120}, {3000, 120}, {2400, 120}, {3000, 120}, {0, 700}, {0, 0}};
static const paso_t p_flujo_detenido[] = {
    {2700, 100}, {0, 100}, {2700, 100}, {0, 100}, {2700, 100}, {0, 900}, {0, 0}};
static const paso_t p_fin_sachet[] = {
    {1500, 200}, {2000, 200}, {2500, 300}, {0, 1500}, {0, 0}};
static const paso_t p_colocacion[] = {
    {1800, 150}, {0, 150}, {1800, 150}, {0, 1200}, {0, 0}};
static const paso_t p_bateria_critica[] = {
    {1000, 500}, {0, 200}, {1000, 500}, {0, 1000}, {0, 0}};
static const paso_t p_desviacion_fuerte[] = {
    {1500, 250}, {2200, 250}, {1500, 250}, {2200, 250}, {0, 900}, {0, 0}};
static const paso_t p_bateria_baja[] = {
    {1000, 700}, {0, 1500}, {0, 0}};

static const paso_t *const patrones[BUZZER_ALARM_CANTIDAD] = {
    [BUZZER_ALARM_VOLUMEN_COMPLETO]  = p_volumen_completo,
    [BUZZER_ALARM_FLUJO_DETENIDO]    = p_flujo_detenido,
    [BUZZER_ALARM_FIN_SACHET]        = p_fin_sachet,
    [BUZZER_ALARM_COLOCACION]        = p_colocacion,
    [BUZZER_ALARM_BATERIA_CRITICA]   = p_bateria_critica,
    [BUZZER_ALARM_DESVIACION_FUERTE] = p_desviacion_fuerte,
    [BUZZER_ALARM_BATERIA_BAJA]      = p_bateria_baja,
};

static const char *const nombres[BUZZER_ALARM_CANTIDAD] = {
    "VOLUMEN_COMPLETO", "FLUJO_DETENIDO", "FIN_SACHET", "COLOCACION",
    "BATERIA_CRITICA", "DESVIACION_FUERTE", "BATERIA_BAJA"};

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

const char *BuzzerAlarm_Nombre(buzzer_alarm_t patron) {
    return (patron < BUZZER_ALARM_CANTIDAD) ? nombres[patron] : "?";
}
