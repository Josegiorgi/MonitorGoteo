/*==================[inclusions]=============================================*/
#include <stddef.h>
#include "pendiente.h"
#include "registro.h"
#include "esp_log.h"

/*==================[macros and definitions]=================================*/
#define US_POR_MINUTO   60000000LL

/*==================[internal data definition]===============================*/
static const char *TAG = "pendiente";
static bool hay = false;               /*!< Hay un registro sin descargar */
static uint8_t estado = REGISTRO_ESTADO_TERMINADA;
static uint32_t base_reportada = 0;    /*!< Interrumpida: minuto del último REINICIO (los minutos informados se cuentan desde ahí) */
static uint32_t congelado = 0;         /*!< Minuto informado cuando llegó el OK */
static bool contando = false;          /*!< true mientras corre el contador de minutos */
static uint32_t base_acumulado = 0;    /*!< Minuto (desde el inicio de la infusión) en el instante de referencia */
static int64_t t_ref_us = 0;           /*!< Instante de referencia del contador */
static uint32_t ultimo_escrito = 0;    /*!< Último minuto que se escribió como muestra en el registro */

/*==================[internal functions definition]==========================*/
static void Guardar(void) {
    registro_pendiente_t p = {
        .estado = estado,
        .pendiente = hay,
        .base_minuto = base_reportada,
        .minuto_congelado = congelado,
    };
    Registro_EscribirPendiente(&p);
}

/** Minutos desde el inicio de la infusión, sin contar el tiempo apagado (no hay RTC) */
static uint32_t Acumulado(int64_t ahora_us) {
    int64_t dt = ahora_us - t_ref_us;
    return base_acumulado + (uint32_t)((dt > 0) ? dt / US_POR_MINUTO : 0);
}

/*==================[external functions definition]==========================*/
void Pendiente_Init(void) {
    registro_pendiente_t p;
    hay = false;
    contando = false;
    if (Registro_LeerPendiente(&p)) {
        estado = p.estado;
        hay = p.pendiente;
        base_reportada = p.base_minuto;
        congelado = p.minuto_congelado;
    }
}

bool Pendiente_AlEncender(bool infusion_sin_terminar, int64_t ahora_us) {
    if (!infusion_sin_terminar && !hay) {
        return false;
    }
    // Último minuto guardado: de la última muestra de la infusión (durante la infusión, o las que se siguen escribiendo después del fin)
    uint32_t ultimo_minuto = 0, id = 0, inicio = 0;
    if (Registro_UltimaInfusion(&id, &inicio)) {
        Registro_UltimaMuestra(inicio, &ultimo_minuto, NULL);
    }
    if (infusion_sin_terminar) {
        estado = REGISTRO_ESTADO_INTERRUMPIDA;   // no se retoma: se la da por interrumpida
        Registro_BorrarInfusion();
    }
    hay = true;
    congelado = 0;
    base_reportada = (estado == REGISTRO_ESTADO_INTERRUMPIDA) ? ultimo_minuto : 0;   // interrumpida: minutos desde este reinicio
    base_acumulado = ultimo_minuto;                                                   // el contador sigue desde el último minuto guardado
    t_ref_us = ahora_us;
    ultimo_escrito = ultimo_minuto;
    contando = true;
    Registro_Anotar(REG_REINICIO, 0, ultimo_minuto, 0, 0);
    Guardar();
    ESP_LOGW(TAG, "registro pendiente de descarga: estado %u, ultimo minuto guardado %lu", estado, (unsigned long)ultimo_minuto);
    return true;
}

void Pendiente_Terminada(uint32_t minuto_fin, int64_t ahora_us) {
    estado = REGISTRO_ESTADO_TERMINADA;
    hay = true;
    congelado = 0;
    base_reportada = 0;
    base_acumulado = minuto_fin;   // el minuto sigue contando desde el fin
    t_ref_us = ahora_us;
    ultimo_escrito = minuto_fin;
    contando = true;
    Guardar();
}

void Pendiente_Tick(int64_t ahora_us) {
    if (!contando) {
        return;
    }
    uint32_t minuto = Acumulado(ahora_us);
    if (minuto > ultimo_escrito) {
        Registro_Muestra(minuto, 0, 0, 0);   // una muestra por minuto: de ahí sigue la cuenta si el equipo se reinicia
        ultimo_escrito = minuto;
    }
}

uint32_t Pendiente_MinutoActual(int64_t ahora_us) {
    if (contando) {
        uint32_t acumulado = Acumulado(ahora_us);
        return (acumulado > base_reportada) ? acumulado - base_reportada : 0;
    }
    return congelado;
}

void Pendiente_Descargado(int64_t ahora_us) {
    if (!hay) {
        return;
    }
    congelado = Pendiente_MinutoActual(ahora_us);
    contando = false;
    hay = false;
    Guardar();
    ESP_LOGI(TAG, "registro descargado (OK de la app): el minuto queda en %lu", (unsigned long)congelado);
}

void Pendiente_Descartar(void) {
    if (!hay && !contando) {
        return;
    }
    contando = false;
    hay = false;
    congelado = 0;
    Guardar();
}

bool Pendiente_Hay(void) {
    return hay;
}

bool Pendiente_EquipoReiniciado(void) {
    return hay && estado == REGISTRO_ESTADO_INTERRUMPIDA;
}

uint8_t Pendiente_Estado(void) {
    return estado;
}
