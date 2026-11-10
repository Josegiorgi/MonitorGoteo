/*==================[inclusions]=============================================*/
#include "comunicacion.h"
#include "ble_goteo.h"
#include "protocolo.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*==================[macros and definitions]=================================*/
#define TAG               "comunicacion"
#define TAREA_STACK       4096
#define TAREA_PRIORIDAD   3       /*!< Baja: no debe quitarle tiempo al muestreo de las gotas (prioridad 5) */
#define COLA_RX_LARGO     8       /*!< Pedazos que puede tener la app esperando (cada uno hasta BLE_GOTEO_RX_MAX bytes) */

/*==================[internal data definition]==========================*/
static QueueHandle_t cola_rx = NULL;
static TaskHandle_t tarea = NULL;
static volatile bool apagado = false;
static protocolo_enlace_t protocolo_enlace;

/*==================[internal functions definition]==========================*/
static bool EnviarPorBle(const uint8_t *datos, size_t largo) {
    return BleGoteo_Enviar(datos, largo);
}

/** Atiende lo que escribe la app. Una orden larga (DESCARGAR) ocupa esta tarea hasta terminar */
static void TareaComunicacion(void *arg) {
    ble_goteo_rx_t rx;
    bool estaba_conectado = false;
    while (!apagado) {
        if (xQueueReceive(cola_rx, &rx, pdMS_TO_TICKS(500)) == pdTRUE) {
            Protocolo_Recibir(rx.datos, rx.largo);
        } else {
            Protocolo_FinDeEntrada();   // 0,5 s sin datos: una orden escrita a mano sin '\n' se ejecuta igual
        }
        bool conectado = BleGoteo_Conectado();
        if (estaba_conectado && !conectado) {
            Protocolo_Reiniciar();   // se cortó: se descarta lo que había quedado a medias
            xQueueReset(cola_rx);
        }
        estaba_conectado = conectado;
    }
    tarea = NULL;
    vTaskDelete(NULL);
}

/*==================[external functions definition]==========================*/
bool Comunicacion_Init(const char *nombre, const comunicacion_enlace_t *enlace) {
    if (tarea != NULL) {
        return true;
    }
    if (nombre == NULL || enlace == NULL || enlace->estado == NULL || enlace->minuto_actual == NULL) {
        return false;
    }
    protocolo_enlace = (protocolo_enlace_t){
        .enviar = EnviarPorBle,
        .estado = enlace->estado,
        .minuto_actual = enlace->minuto_actual,
        .descargado = enlace->descargado,
    };
    Protocolo_Init(&protocolo_enlace);

    cola_rx = xQueueCreate(COLA_RX_LARGO, sizeof(ble_goteo_rx_t));
    if (cola_rx == NULL) {
        return false;
    }
    apagado = false;
    if (!BleGoteo_Init(nombre, cola_rx)) {
        return false;
    }
    if (xTaskCreate(TareaComunicacion, "comunicacion", TAREA_STACK, NULL, TAREA_PRIORIDAD, &tarea) != pdPASS) {
        ESP_LOGE(TAG, "no se pudo crear la tarea de comunicacion");
        return false;
    }
    return true;
}

void Comunicacion_Apagar(void) {
    apagado = true;
    BleGoteo_Apagar();
    vTaskDelay(pdMS_TO_TICKS(600));   // la tarea sale en su próxima vuelta (espera como máximo 500 ms)
}

bool Comunicacion_Conectado(void) {
    return BleGoteo_Conectado();
}
