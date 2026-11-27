/*==================[inclusions]=============================================*/
#include "infusion.h"
#include "estado.h"
#include "escenarios.h"
#include "pendiente.h"
#include "goteo.h"
#include "alarmas.h"
#include "buzzer_alarm.h"
#include "registro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define COLA_GOTAS_LARGO      32

static const char *TAG = "infusion";

/*==================[internal data definition]===============================*/
static QueueHandle_t cola_gotas = NULL;   /*!< Hora de cada gota detectada (de la tarea del sensor al main) */
static bool tiempo_iniciado = false;   /*!< true desde que arrancó el tiempo de la infusión (la primera vez que se entra al Monitoreo) */
static int64_t ultimo_guardado_us = 0; /*!< Última vez que se escribió la muestra de 1 min en el registro */
static uint32_t inicio_linea_activa = 0; /*!< Línea del registro donde empezó la infusión activa */
static uint32_t id_activa = 0;         /*!< ID de la infusión activa (o de la última) */
static uint16_t duracion_guardada = 0; /*!< Duración en curso (para anotar CAMBIO_DATOS con el valor anterior) */
static uint8_t direccion_desvio[ALARMAS_CANTIDAD];   /*!< 0 = alto, 1 = bajo: sentido del desvío cuando se activó (para el registro) */
static int64_t ajuste_ini_us = 0;      /*!< Hora en que se entró al Ajuste de goteo (para el recordatorio de los 10 min) */
static int64_t ultimo_recordatorio_us = 0;

/*==================[internal functions definition]==========================*/
/** Guarda en la NVS la infusión activa (configuración y dónde empieza en el registro). Se escribe pocas veces por
 * infusión: al INICIO y en CAMBIO_DATOS */
static void GuardarInfusion(void) {
    registro_infusion_t inf = {
        .factor_goteo = datos.factor_goteo,
        .volumen_ml = datos.volumen_total_ml,
        .duracion_min = datos.duracion_min,
        .inicio_linea = inicio_linea_activa,
        .id = id_activa,
    };
    Registro_GuardarInfusion(&inf);
}

/** Escribe en el registro la muestra de cada 1 min (contador de gotas, caudal x10 y volumen x10) */
static void GuardarMuestra(int64_t ahora_us) {
    uint32_t gotas = Goteo_Gotas();
    uint32_t caudal_x10 = (uint32_t)datos.flujo_mlh * 10;
    uint32_t volumen_x10 = gotas * 10 / (datos.factor_goteo ? datos.factor_goteo : 1);
    Registro_Muestra(Infusion_Minuto(ahora_us), gotas, (caudal_x10 > UINT16_MAX) ? UINT16_MAX : (uint16_t)caudal_x10,
                     (volumen_x10 > UINT16_MAX) ? UINT16_MAX : (uint16_t)volumen_x10);
    ultimo_guardado_us = ahora_us;
}

/** Empieza a contar gotas. Se llama al entrar al Ajuste de goteo: desde que se abre el clamp ya
 * pasa líquido al paciente, así que el volumen cuenta desde acá. El tiempo, no (ver Monitoreo). */
static void IniciarMedicion(void) {
    int64_t ahora = esp_timer_get_time();
    Goteo_Iniciar(datos.factor_goteo, ahora);
    Alarmas_Iniciar(datos.factor_goteo, datos.volumen_total_ml, datos.duracion_min, ahora);
    datos.umbral_sin_gotas_s = (uint16_t)Alarmas_TOclusionS();
    midiendo = true;
    Pendiente_Descartar();   // una infusión nueva deja de lado el registro sin descargar de la anterior
    datos.registro_pendiente = false;
    duracion_guardada = datos.duracion_min;
    inicio_linea_activa = Registro_Total();                                          // dónde empieza esta infusión en el registro
    id_activa = Registro_NuevoId();                                                  // ID propio de esta infusión
    Registro_AnotarConDato(REG_INICIO, 0, 0, datos.factor_goteo, datos.volumen_total_ml, id_activa);   // se abre el registro de la infusión
    Registro_Anotar(REG_DURACION, 0, 0, datos.duracion_min, 0);
    GuardarInfusion();                                                              // y se guarda como infusión activa
    ultimo_guardado_us = ahora;
}

/*==================[external functions definition]==========================*/
void Infusion_Init(void) {
    cola_gotas = xQueueCreate(COLA_GOTAS_LARGO, sizeof(int64_t));
}

void Infusion_AvisoGota(int64_t ahora_us) {
    xQueueSend(cola_gotas, &ahora_us, 0);   // si la cola está llena se pierde la gota
}

bool Infusion_EnCurso(void) {
    return midiendo && datos.infusion_configurada;
}

uint16_t Infusion_Minuto(int64_t ahora_us) {
    if (!tiempo_iniciado) {
        return 0;
    }
    uint32_t m = Goteo_TiempoS(ahora_us) / 60;
    return (m > UINT16_MAX) ? UINT16_MAX : (uint16_t)m;
}

void Infusion_EventoAlarma(alarma_evento_t evento, bool activo, int64_t ahora_us) {
    if (activo && (evento == ALARMA_DESVIACION_FUERTE || evento == AVISO_DESVIACION_LEVE)) {
        direccion_desvio[evento] = (Alarmas_Desvio() < 0.0f) ? 1 : 0;   // 0 = flujo alto, 1 = flujo bajo (lo pide la app)
    }
    Registro_Anotar(REG_ALARMA, activo, Infusion_Minuto(ahora_us), (uint16_t)evento, direccion_desvio[evento]);
    ESP_LOGI(TAG, "%s %s", Alarmas_Nombre(evento), activo ? "ACTIVADA" : "resuelta");
}

void Infusion_AnotarSilenciada(int evento, int64_t ahora_us) {
    Registro_Anotar(REG_SILENCIADO, 0, Infusion_Minuto(ahora_us), (uint16_t)evento, direccion_desvio[evento]);
}

void Infusion_EventoClave(ui_clave_resultado_t resultado) {
    registro_evento_t e = (resultado == UI_CLAVE_OK) ? REG_CLAVE_OK : (resultado == UI_CLAVE_ERROR) ? REG_CLAVE_ERROR : REG_TECNICO_OK;
    Registro_Anotar(e, 0, Infusion_Minuto(esp_timer_get_time()), 0, 0);
}

void Infusion_GuardarMuestraSiToca(int64_t ahora_us) {
    if (midiendo && datos.infusion_configurada && tiempo_iniciado && (ahora_us - ultimo_guardado_us) >= (int64_t)GUARDAR_CADA_S * 1000000) {
        GuardarMuestra(ahora_us);    // cada 1 min: así un reinicio pierde como mucho un minuto de la cuenta
    }
}

void Infusion_RevisarTransiciones(ui_pantalla_t antes) {
    ui_pantalla_t ahora = UI_PantallaActual();
    if (ahora == UI_PANTALLA_AJUSTE_GOTEO && antes != UI_PANTALLA_AJUSTE_GOTEO) {
        ajuste_ini_us = esp_timer_get_time();
        ultimo_recordatorio_us = 0;
    }
    if (antes == UI_PANTALLA_RESUMEN && ahora == UI_PANTALLA_AJUSTE_GOTEO) {
        if (!midiendo) {
            IniciarMedicion();
        } else {
            // Ajustes > Cambiar datos: sigue la misma infusión (no se reinicia el contador), cambian el objetivo y T_ocl
            Alarmas_CambiarDuracion(datos.duracion_min);
            datos.umbral_sin_gotas_s = (uint16_t)Alarmas_TOclusionS();
            Registro_Anotar(REG_CAMBIO_DATOS, 0, Infusion_Minuto(esp_timer_get_time()), duracion_guardada, datos.duracion_min);
            duracion_guardada = datos.duracion_min;
            GuardarInfusion();
        }
    } else if (antes == UI_PANTALLA_AJUSTE_GOTEO && ahora == UI_PANTALLA_MONITOREO) {
        if (!tiempo_iniciado) {
            Goteo_ReiniciarTiempo(esp_timer_get_time());   // el tiempo de la infusión arranca recién acá
            tiempo_iniciado = true;
        }
        Registro_Anotar(REG_AJUSTE_OK, 0, Infusion_Minuto(esp_timer_get_time()), 0, 0);
    }
    if (!datos.infusion_configurada && midiendo) {
        // Se detuvo el monitoreo (o se apagó): se anota el fin, se borra la infusión activa, se dejan de medir gotas
        // y se apaga lo que estuviera sonando
        const int64_t ahora_fin = esp_timer_get_time();
        const uint32_t minuto_fin = Infusion_Minuto(ahora_fin);
        Registro_AnotarConDato(REG_FIN, 0, minuto_fin, Goteo_VolumenMl(), 0, Goteo_Gotas());   // volumen y gotas totales
        Registro_BorrarInfusion();
        Pendiente_Terminada(minuto_fin, ahora_fin);   // el minuto sigue contando hasta que la app confirme la descarga (OK)
        datos.registro_pendiente = true;               // aviso: registro pendiente de descarga
        midiendo = false;
        tiempo_iniciado = false;
        Alarmas_Iniciar(datos.factor_goteo, datos.volumen_total_ml, datos.duracion_min, esp_timer_get_time());   // pone todos los eventos en cero
        AlarmasSonido_Actualizar();
    }
}

bool Infusion_SinInfusionEvaluable(void) {
    if (midiendo || datos.infusion_configurada) {
        return false;
    }
    switch (UI_PantallaActual()) {
    case UI_PANTALLA_FIN:
    case UI_PANTALLA_ALARMA:
        return true;
    case UI_PANTALLA_ACCESO:
    case UI_PANTALLA_APAGAR:
    case UI_PANTALLA_PENDIENTE:
        // La enfermera está ahí (clave, menú): no se cuentan gotas nuevas, pero una alarma que ya está activa sigue evaluándose
        return Alarmas_Activas() != 0;
    default:
        return false;
    }
}

int Infusion_ProcesarGotas(void) {
    int64_t t_us;
    int n = 0;
    while (xQueueReceive(cola_gotas, &t_us, 0) == pdTRUE) {
        if (midiendo) {
            Goteo_Gota(t_us);
            Alarmas_Gota(t_us);
            n++;
            ESP_LOGI(TAG, "gota %lu", (unsigned long)Goteo_Gotas());
        } else if (Infusion_SinInfusionEvaluable()) {
            Alarmas_GotaSinMonitoreo(t_us);   // gotas sin infusión activa: si son continuas 2 min, alarma
        }
    }
    return n;
}

void Infusion_ActualizarMedicion(int64_t ahora_us) {
    if (!midiendo) {
        return;
    }
    datos.volumen_ml = Goteo_VolumenMl();
    datos.flujo_mlh = Goteo_CaudalMlh(ahora_us);
    datos.segundos_sin_gotas = Goteo_SegundosSinGotas(ahora_us);
    datos.tiempo_s = (UI_PantallaActual() == UI_PANTALLA_AJUSTE_GOTEO) ? 0 : Goteo_TiempoS(ahora_us);
}

void Infusion_RecordarAjuste(int64_t ahora_us) {
    bool recordar = UI_PantallaActual() == UI_PANTALLA_AJUSTE_GOTEO && (ahora_us - ajuste_ini_us) > (int64_t)AJUSTE_RECORDATORIO_S * 1000000;
    datos.recordatorio_ajuste = recordar;
    if (recordar && (ultimo_recordatorio_us == 0 || (ahora_us - ultimo_recordatorio_us) >= (int64_t)AJUSTE_BEEP_CADA_S * 1000000)) {
        ultimo_recordatorio_us = ahora_us;
        BuzzerAlarm_Efecto(BUZZER_EFECTO_CLICK);
    }
}
