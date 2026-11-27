/*==================[inclusions]=============================================*/
#include "modo_tecnico.h"
#include "estado.h"
#include "reposo.h"
#include "pendiente.h"
#include "ui.h"
#include "buzzer_alarm.h"
#include "optical_sensor.h"
#include "bateria.h"
#include "registro.h"
#include "esp_log.h"

/*==================[macros and definitions]=================================*/
#define FIRMWARE_VERSION      "0.1"   /*!< Versión que muestra el Diagnóstico del Modo Técnico */

static const char *TAG = "modo_tecnico";

/*==================[internal data definition]===============================*/
static uint8_t prueba_sonando = 0;     /*!< Modo Técnico: patrón que está sonando (0 = ninguno, n = patrón n-1) */

/*==================[external functions definition]==========================*/
void ModoTecnico_Atender(void) {
    if (datos.probar_alarma != prueba_sonando) {
        prueba_sonando = datos.probar_alarma;
        if (prueba_sonando == 0) {
            BuzzerAlarm_Stop();
        } else if (prueba_sonando - 1 < BUZZER_ALARM_CANTIDAD) {
            BuzzerAlarm_Start((buzzer_alarm_t)(prueba_sonando - 1));
        }
    }
    if (datos.borrar_registro) {
        datos.borrar_registro = false;
        UI_PantallaMensaje("Borrando registro...");   // la partición tarda ~10 s en borrarse
        Registro_Borrar();
        Pendiente_Descartar();   // sin registro no queda nada pendiente de descarga
        datos.registro_pendiente = false;
        ESP_LOGW(TAG, "registro borrado desde el Modo Tecnico");
    }
    if (UI_SalirPedido()) {
        BuzzerAlarm_Stop();
        Reposo_ReiniciarSinEspera();
    }
}

void ModoTecnico_ActualizarDiagnostico(void) {
    datos.base_optica_mv[0] = (uint16_t)OpticalSensor_BaseMv(0);
    datos.base_optica_mv[1] = (uint16_t)OpticalSensor_BaseMv(1);
    datos.bateria_mv = Bateria_Mv();
    datos.registro_lineas = (uint16_t)Registro_Total();
    datos.registro_descargado = Registro_Descargado();
    datos.version = FIRMWARE_VERSION;
}
