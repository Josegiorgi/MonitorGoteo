/*==================[inclusions]=============================================*/
#include "descarga.h"
#include "estado.h"
#include "escenarios.h"
#include "infusion.h"
#include "gestion_alarmas.h"
#include "pendiente.h"
#include "registro.h"
#if USAR_BLE
#include "comunicacion.h"
#endif
#include "esp_log.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define NOMBRE_BLE            "MonitorGoteo-01"   /*!< Nombre del equipo para la app: cambiarlo en cada equipo (hasta 20 caracteres) */

#if !USAR_BLE
static const char *TAG = "descarga";
#endif

/*==================[internal data definition]===============================*/
static volatile bool ok_recibido = false;   /*!< La app confirmó la descarga (OK por BLE): lo atiende el lazo principal */

/*==================[internal functions definition]==========================*/
#if USAR_BLE
/** Lo que le informamos a la comunicación BLE (componente aparte): estado de la última infusión, el minuto actual y el OK */
static uint8_t EstadoParaBle(void) {
    if (Infusion_EnCurso()) {
        return REGISTRO_ESTADO_EN_CURSO;       // 1 = en curso
    }
    return Pendiente_Estado();                 // 0 = terminada, 2 = interrumpida por un reinicio
}

/** Minuto actual: en curso = desde el inicio; terminada = desde el inicio (sigue contando hasta el OK); interrumpida = desde el reinicio */
static uint32_t MinutoParaBle(void) {
    if (Infusion_EnCurso()) {
        return Infusion_Minuto(esp_timer_get_time());
    }
    return Pendiente_MinutoActual(esp_timer_get_time());
}

static void DescargadoParaBle(void) {
    ok_recibido = true;   // se atiende en el lazo principal (esta función corre en la tarea de comunicación)
}

static const comunicacion_enlace_t enlace_ble = {
    .estado = EstadoParaBle,
    .minuto_actual = MinutoParaBle,
    .descargado = DescargadoParaBle,
};
#endif

/*==================[external functions definition]==========================*/
void Descarga_IniciarBle(void) {
#if USAR_BLE
    Comunicacion_Init(NOMBRE_BLE, &enlace_ble);   // BLE con la app
#else
    ESP_LOGW(TAG, "SIN Bluetooth (USAR_BLE = 0)");
#endif
}

void Descarga_Atender(int64_t ahora) {
    Pendiente_Tick(ahora);
    if (ok_recibido) {
        ok_recibido = false;
        Pendiente_Descargado(ahora);   // la app confirmó la descarga: se deja de contar y se apaga el aviso y la alarma de reinicio
        if (!midiendo) {
            GestionAlarmas_Evaluar(ahora, true);
        }
    }
    if (datos.descartar_pendiente) {
        datos.descartar_pendiente = false;
        Pendiente_Descartar();         // "Continuar igual": el registro sin descargar queda de lado
        if (!midiendo) {
            GestionAlarmas_Evaluar(ahora, true);
        }
    }
    datos.registro_pendiente = Pendiente_Hay();
}
