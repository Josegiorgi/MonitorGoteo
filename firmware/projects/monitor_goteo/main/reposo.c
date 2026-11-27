/*==================[inclusions]=============================================*/
#include "reposo.h"
#include "escenarios.h"
#include "pendiente.h"
#include "ui.h"
#include "encoder.h"
#include "buzzer_alarm.h"
#include "optical_sensor.h"
#if USAR_BLE
#include "comunicacion.h"
#endif
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_attr.h"

/*==================[macros and definitions]=================================*/
#define MARCA_SIN_ESPERA      0x5AFE0B07   /*!< Marca (en memoria que sobrevive al reinicio) para no pedir de nuevo el botón 3 s después de salir del reposo o del Modo Técnico */

static const char *TAG = "reposo";

/*==================[internal data definition]===============================*/
RTC_NOINIT_ATTR static uint32_t marca_sin_espera;   /*!< MARCA_SIN_ESPERA si el reinicio fue a propósito (reposo o Salir del Modo Técnico) */

/*==================[external functions definition]==========================*/
bool Reposo_ReinicioAProposito(void) {
    bool a_proposito = (marca_sin_espera == MARCA_SIN_ESPERA);
    marca_sin_espera = 0;
    return a_proposito;
}

void Reposo_ReiniciarSinEspera(void) {
    marca_sin_espera = MARCA_SIN_ESPERA;
    esp_restart();
}

void Reposo_Entrar(void) {
    Pendiente_Tick(esp_timer_get_time());   // con un registro pendiente, deja guardado el último minuto antes de dormir
#if USAR_BLE
    Comunicacion_Apagar();   // el BLE se apaga antes del light sleep
#endif
    BuzzerAlarm_Stop();
    OpticalSensor_Stop();
    UI_PantallaApagada();
    ESP_LOGI(TAG, "reposo: mantener el boton %d ms para encender", ENCENDIDO_MS);
    Encoder_DormirHastaMantenido(ENCENDIDO_MS);
    Reposo_ReiniciarSinEspera();
}
