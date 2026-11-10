/*==================[inclusions]=============================================*/
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "registro.h"
#include "comunicacion.h"
#include "alarmas.h"

/** \brief Prueba del Bluetooth (componente comunicacion) sin el resto del firmware: sin pantalla, sensor ni alarmas.
 *
 * Al arrancar borra el registro (unos 10 s) y carga una infusión de EJEMPLO, para probar con nRF Connect (o con la app) las
 * órdenes HORA, PACIENTE, ESTADO, DESCARGAR y OK sin esperar a que pase una infusión real. El estado de la infusión de ejemplo
 * se elige con ESCENARIO_BLE:
 *
 * | ESCENARIO_BLE            | Qué simula                                                                                        |
 * |:-------------------------|:--------------------------------------------------------------------------------------------------|
 * | BLE_EN_CURSO             | Infusión en curso (ESTADO,<ID>,1,...): el minuto sube y no hay línea FIN                             |
 * | BLE_TERMINADA            | Infusión terminada y sin descargar (ESTADO,<ID>,0,...): tiene FIN y el minuto sigue subiendo hasta el OK |
 * | BLE_INTERRUMPIDA         | Infusión interrumpida por un reinicio (ESTADO,<ID>,2,...): termina en REINICIO y el minuto cuenta desde el reinicio |
 * | BLE_SIN_INFUSION         | Registro vacío: ESTADO y DESCARGAR responden ERR,SIN_REGISTRO                                        |
 *
 * Con SEGUNDOS_POR_MINUTO = 10 cada "minuto" dura 10 s (para no esperar); poner 60 para minutos reales.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 10/11/2026 | Document creation                              |
 */

/*==================[macros and definitions]=================================*/
#define BLE_EN_CURSO        1
#define BLE_TERMINADA       2
#define BLE_INTERRUMPIDA    3
#define BLE_SIN_INFUSION    4

#define ESCENARIO_BLE       BLE_SIN_INFUSION   /*!< Qué infusión de ejemplo se carga al arrancar */
#define SEGUNDOS_POR_MINUTO 10              /*!< Duración de cada "minuto" simulado (60 = minutos reales) */
#define NOMBRE_BLE          "MonitorGoteo-01"

#define MINUTO_EN_CURSO     7    /*!< Minuto en que está la infusión de ejemplo en curso */
#define MINUTO_FIN          18   /*!< Minuto del FIN de la infusión terminada */
#define MINUTO_REINICIO     9    /*!< Último minuto guardado antes del reinicio de la infusión interrumpida */

/*==================[internal data definition]===============================*/
static const char *TAG = "test_ble";
static uint8_t estado_sim = REGISTRO_ESTADO_EN_CURSO;   /*!< Lo que informa ESTADO */
static uint32_t minuto_base = 0;       /*!< Minuto informado en el instante t_ref */
static uint32_t base_reportada = 0;    /*!< Interrumpida: minuto del REINICIO (los minutos se cuentan desde el reinicio) */
static int64_t t_ref_us = 0;
static bool contando = true;
static uint32_t congelado = 0;

/*==================[internal functions definition]==========================*/
static uint32_t Minuto(void) {
    if (!contando) {
        return congelado;
    }
    return minuto_base + (uint32_t)((esp_timer_get_time() - t_ref_us) / ((int64_t)SEGUNDOS_POR_MINUTO * 1000000));
}

static uint8_t EstadoSim(void) {
    return estado_sim;
}

static uint32_t MinutoSim(void) {
    return Minuto();
}

/** Como el firmware real: con el OK deja de contar el minuto y la infusión queda descargada */
static void Descargado(void) {
    if (estado_sim == REGISTRO_ESTADO_EN_CURSO || !contando) {
        return;
    }
    congelado = Minuto();
    contando = false;
    registro_pendiente_t p = {.estado = estado_sim, .pendiente = false, .base_minuto = base_reportada, .minuto_congelado = congelado};
    Registro_EscribirPendiente(&p);
    ESP_LOGI(TAG, "OK recibido: el minuto queda en %lu", (unsigned long)congelado);
}

static void Muestras(uint32_t desde, uint32_t hasta) {
    for (uint32_t m = desde; m <= hasta; m++) {
        Registro_Muestra(m, m * 20, 625, (uint16_t)(m * 10));   // 62,5 mL/h y 1 mL por minuto
    }
}

/** Carga en el registro una infusión de ejemplo con alarma, silenciado, clave incorrecta y cambio de datos */
static void CargarEjemplo(void) {
    ESP_LOGW(TAG, "borrando el registro para cargar la infusion de ejemplo (unos 10 s)...");
    Registro_Borrar();
    Registro_Init();
    if (ESCENARIO_BLE == BLE_SIN_INFUSION) {
        estado_sim = REGISTRO_ESTADO_EN_CURSO;
        return;
    }

    uint32_t id = Registro_NuevoId();
    registro_infusion_t inf = {.factor_goteo = 20, .volumen_ml = 500, .duracion_min = 480, .inicio_linea = Registro_Total(), .id = id};
    Registro_AnotarConDato(REG_INICIO, 0, 0, 20, 500, id);
    Registro_Anotar(REG_DURACION, 0, 0, 480, 0);
    Registro_GuardarInfusion(&inf);
    Registro_Anotar(REG_AJUSTE_OK, 0, 0, 0, 0);

    Muestras(1, 2);
    Registro_Anotar(REG_ALARMA, 1, 3, ALARMA_FLUJO_DETENIDO, 0);
    Muestras(3, 3);
    Registro_Anotar(REG_SILENCIADO, 0, 4, ALARMA_FLUJO_DETENIDO, 0);
    Muestras(4, 4);
    Registro_Anotar(REG_ALARMA, 0, 5, ALARMA_FLUJO_DETENIDO, 0);
    Muestras(5, 5);
    Registro_Anotar(REG_CLAVE_ERROR, 0, 6, 0, 0);
    Muestras(6, 6);
    Registro_Anotar(REG_CAMBIO_DATOS, 0, 7, 480, 360);
    Muestras(7, 7);

    registro_pendiente_t p = {.estado = REGISTRO_ESTADO_EN_CURSO, .pendiente = false};
#if ESCENARIO_BLE == BLE_EN_CURSO
    estado_sim = REGISTRO_ESTADO_EN_CURSO;
    minuto_base = MINUTO_EN_CURSO;
#elif ESCENARIO_BLE == BLE_TERMINADA
    Muestras(8, MINUTO_FIN - 1);
    Registro_AnotarConDato(REG_FIN, 0, MINUTO_FIN, 32, 0, 640);   // 32 mL y 640 gotas
    Registro_BorrarInfusion();
    p.estado = REGISTRO_ESTADO_TERMINADA;
    p.pendiente = true;
    estado_sim = REGISTRO_ESTADO_TERMINADA;
    minuto_base = MINUTO_FIN;   // el minuto sigue contando desde el fin hasta el OK
#elif ESCENARIO_BLE == BLE_INTERRUMPIDA
    Muestras(8, MINUTO_REINICIO);
    Registro_Anotar(REG_REINICIO, 0, MINUTO_REINICIO, 0, 0);
    Registro_BorrarInfusion();
    p.estado = REGISTRO_ESTADO_INTERRUMPIDA;
    p.pendiente = true;
    p.base_minuto = MINUTO_REINICIO;
    estado_sim = REGISTRO_ESTADO_INTERRUMPIDA;
    base_reportada = MINUTO_REINICIO;
    minuto_base = 0;            // los minutos se cuentan desde el reinicio
#endif
    Registro_EscribirPendiente(&p);
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    if (!Registro_Init()) {
        ESP_LOGE(TAG, "el registro no responde (NVS o particion): no se puede probar");
        return;
    }
    CargarEjemplo();
    t_ref_us = esp_timer_get_time();

    static const comunicacion_enlace_t enlace = {
        .estado = EstadoSim,
        .minuto_actual = MinutoSim,
        .descargado = Descargado,
    };
    Comunicacion_Init(NOMBRE_BLE, &enlace);

    Registro_ActivarLog(true);   // lo que se anote durante la prueba (por ejemplo la linea DESCARGA) sale por la consola
    Registro_Volcar(200);
    ESP_LOGI(TAG, "listo: infusion de ejemplo cargada (%lu lineas). Conectar con nRF Connect a %s", (unsigned long)Registro_Total(), NOMBRE_BLE);

    while (true) {
        ESP_LOGI(TAG, "estado %u, minuto %lu, %s", estado_sim, (unsigned long)Minuto(), Comunicacion_Conectado() ? "app conectada" : "sin conexion");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
