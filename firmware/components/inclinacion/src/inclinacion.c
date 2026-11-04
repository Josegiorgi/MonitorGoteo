/*==================[inclusions]=============================================*/
#include <math.h>
#include "inclinacion.h"
#include "adxl345.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/
#define TAREA_STACK        3072
#define TAREA_PRIORIDAD    3       /*!< Menor que la del muestreo del sensor óptico (5): leer el I2C no tiene que atrasarlo */
#define LECTURA_ESPERA_MS  10      /*!< Entre lecturas: el ADXL345 entrega una muestra nueva cada 10 ms (100 Hz) */
#define REINTENTO_MS       200     /*!< Espera tras una lectura fallida antes de reintentar */
#define RAD_TO_DEG         57.29578f

/*==================[internal data definition]===============================*/
static TaskHandle_t tarea = NULL;
static volatile float grados = 0.0f;        /*!< float de 32 bits: la lectura es atómica en el ESP32-C3 */
static volatile bool valida = false;

/*==================[internal functions definition]==========================*/
/** Promedia INCLINACION_LECTURAS lecturas del acelerómetro (en g). false si alguna falló. */
static bool LeerPromedio(float *ax, float *ay, float *az) {
    float sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < INCLINACION_LECTURAS; i++) {
        float x, y, z;
        if (!ADXL345_ReadAccel(&x, &y, &z)) {
            return false;
        }
        sx += x;
        sy += y;
        sz += z;
        vTaskDelay(pdMS_TO_TICKS(LECTURA_ESPERA_MS));
    }
    *ax = sx / INCLINACION_LECTURAS;
    *ay = sy / INCLINACION_LECTURAS;
    *az = sz / INCLINACION_LECTURAS;
    return true;
}

static void TareaInclinacion(void *arg) {
    uint8_t fallas = 0;
    while (1) {
        float ax, ay, az;
        if (LeerPromedio(&ax, &ay, &az)) {
            fallas = 0;
            float norma = sqrtf(ax * ax + ay * ay + az * az);
            if (norma > 0.2f) {   // con menos de 0.2 g no hay gravedad con la que medir
                float s = az / norma;
                s = (s > 1.0f) ? 1.0f : (s < -1.0f) ? -1.0f : s;
                grados = asinf(s) * RAD_TO_DEG - INCLINACION_OFFSET_DEG;
                valida = true;
            } else {
                valida = false;
            }
            vTaskDelay(pdMS_TO_TICKS(INCLINACION_PERIODO_MS));
        } else {
            // Falló la lectura (NACK, cable flojo): no se reintenta enseguida para no martillar un bus
            // que puede estar en mal estado. Tras varias seguidas el ángulo deja de ser válido, así no
            // se confunde un valor viejo con uno actual.
            if (fallas < 255) {
                fallas++;
            }
            if (fallas >= INCLINACION_FALLAS_MAX) {
                valida = false;
            }
            vTaskDelay(pdMS_TO_TICKS(REINTENTO_MS));
        }
    }
}

/*==================[external functions definition]==========================*/
bool Inclinacion_Init(void) {
    if (tarea != NULL) {
        return true;
    }
    if (!ADXL345_Init()) {
        return false;
    }
    return xTaskCreate(TareaInclinacion, "inclinacion", TAREA_STACK, NULL, TAREA_PRIORIDAD, &tarea) == pdPASS;
}

bool Inclinacion_Valida(float *g) {
    if (g != NULL) {
        *g = grados;
    }
    return valida;
}
