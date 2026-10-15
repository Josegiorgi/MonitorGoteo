/*! @mainpage Prueba de interfaz de usuario
 *
 * \section genDesc General Description
 *
 * Prueba del módulo interfaz_usuario: todas las pantallas de la OLED manejadas con el encoder.
 *
 * Al arrancar hay que mantener apretado el botón 3 s ("encendido"). Sigue la pantalla de
 * inicio, que hace de autotest y no avanza hasta que todos los chequeos den OK. Después:
 * Autotest -> Colocación -> Config -> Resumen -> Ajuste de goteo -> Monitoreo (la primera
 * configuración no pide código). Con la infusión en curso, el usuario solo puede silenciar la
 * alarma con un click; el doble click abre el Acceso técnico (código 1234) y el Menú técnico
 * (configurar parámetros o apagar el equipo).
 *
 * Como todavía no existe el módulo de sensado, SimularSensado() inventa las mediciones. Para ver
 * las pantallas de alarma, con PROBAR_ALARMAS en 1 el equipo salta directo al Monitoreo después
 * del autotest, y girar el encoder ahí pasa por todas las alarmas (ninguna, oclusión,
 * desviación de flujo, fin de sachet, batería baja, colocación). Pulsar silencia la alarma
 * (queda el aviso abajo en el Monitoreo); en fin de sachet pasa a la pantalla de fin.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 15/10/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "encoder.h"
#include "ui.h"

/*==================[macros and definitions]=================================*/
#define ENCENDIDO_MS          3000  /*!< Tiempo que hay que mantener apretado el botón para encender */
#define DURACION_INICIO_MS    5000  /*!< Duración mínima de la pantalla de inicio (autotest) */
#define PERIODO_CHEQUEO_MS    1000  /*!< Cada cuánto se hace o reintenta un chequeo del autotest */
#define PERIODO_ANIMACION_MS  42    /*!< Cada cuánto avanza un cuadro de la animación del inicio (como en Lopaka) */
#define PERIODO_TICK_MS       1000  /*!< Cada cuánto se simula el sensado y se actualiza la interfaz */

#define TOLERANCIA_GRADOS     2.5f  /*!< Ángulo máximo respecto de la vertical para dar la colocación por correcta */

#define PROBAR_ALARMAS        1     /*!< 1: salta al Monitoreo y girar el encoder recorre las alarmas; 0: flujo normal */

static const char *TAG = "prueba_interfaz";

/*==================[internal data definition]===============================*/
static ui_datos_t datos;
static ui_chequeo_t chequeos[UI_CHEQUEOS_MAX] = {
    {"OLED", UI_CHEQUEO_PENDIENTE},
    {"ENC", UI_CHEQUEO_PENDIENTE},
    {"ACEL", UI_CHEQUEO_PENDIENTE},
    {"FOTO", UI_CHEQUEO_PENDIENTE},
};
static bool encoder_ok = false;

/*==================[internal functions definition]==========================*/
static void DatosIniciales(void) {
    datos = (ui_datos_t){
        .factor_goteo = 20,
        .volumen_total_ml = 500,
        .duracion_min = 8 * 60,
        .flujo_objetivo_mlh = (500 * 60 + 240) / (8 * 60),   // volumen / tiempo; después lo recalcula la interfaz al cambiar Config
        .umbral_sin_gotas_s = 60,
        .bateria_pct = 70,
        .angulo_grados = 1.5f,
        .colocacion_ok = true,
        .tolerancia_grados = TOLERANCIA_GRADOS,
    };
}

/* Chequeos del autotest. Cada uno devuelve true si el componente responde. */
static bool ChequearOled(void) {
    return true;   // si la OLED no respondiera, el equipo ya se habría reiniciado (ver u8g2_esp32_hal)
}

static bool ChequearEncoder(void) {
    return encoder_ok;
}

static bool ChequearAcelerometro(void) {
    return true;   // TODO: leer DEVID (0xE5) del ADXL345 cuando llegue el chip y exista el módulo de sensado
}

static bool ChequearFotodiodos(void) {
    return true;   // TODO: verificar el nivel base de los dos canales del ADC con el módulo de sensado
}

static bool (*const funciones_chequeo[UI_CHEQUEOS_MAX])(void) = {
    ChequearOled, ChequearEncoder, ChequearAcelerometro, ChequearFotodiodos,
};

static bool TodosOk(void) {
    for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
        if (chequeos[i].estado != UI_CHEQUEO_OK) {
            return false;
        }
    }
    return true;
}

/** Autotest de arranque: la pantalla "Iniciando" queda con la animación esperando hasta que
 * todos los chequeos den OK, y como mínimo DURACION_INICIO_MS. Un chequeo que falla queda como
 * ERR y se reintenta cada segundo (por ejemplo, un sensor que se conecta tarde). */
static void AutotestInicio(void) {
    uint32_t proximo_ms[UI_CHEQUEOS_MAX];
    uint32_t t_ms;
    int64_t inicio_us = esp_timer_get_time();

    for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
        proximo_ms[i] = (i + 1) * PERIODO_CHEQUEO_MS;   // se escalonan para que se vea el avance
    }

    do {
        t_ms = (esp_timer_get_time() - inicio_us) / 1000;
        for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
            if (chequeos[i].estado != UI_CHEQUEO_OK && t_ms >= proximo_ms[i]) {
                chequeos[i].estado = funciones_chequeo[i]() ? UI_CHEQUEO_OK : UI_CHEQUEO_FALLA;
                proximo_ms[i] = t_ms + PERIODO_CHEQUEO_MS;
            }
        }
        // El cuadro sale del tiempo transcurrido: dibujar la OLED por I2C tarda (unos 200 ms
        // por pantalla a 50 kHz), así la animación mantiene su velocidad y solo se saltea cuadros
        UI_PantallaInicio(t_ms / PERIODO_ANIMACION_MS);
    } while (t_ms < DURACION_INICIO_MS || !TodosOk());
}

/** Inventa las mediciones que después dará el módulo de sensado, una vez por segundo. */
static void SimularSensado(void) {
    // Monitoreo: el tiempo avanza y el caudal medido es el objetivo
    if (datos.infusion_configurada && !datos.en_pausa && datos.alarma == UI_ALARMA_NINGUNA &&
        UI_PantallaActual() != UI_PANTALLA_AJUSTE_GOTEO) {
        datos.tiempo_s++;
        datos.flujo_mlh = datos.flujo_objetivo_mlh;
        if (datos.volumen_ml + 1 < datos.volumen_total_ml) {
            datos.volumen_ml++;
        }
    }
}

#if PROBAR_ALARMAS
/** Pasa a la siguiente (o anterior) alarma de la lista y carga los datos que cada una muestra. */
static void CambiarAlarmaDePrueba(int8_t sentido) {
    const uint8_t cantidad = UI_ALARMA_COLOCACION + 1;
    datos.alarma = (datos.alarma + cantidad + sentido) % cantidad;

    // Valores de muestra: todo normal salvo lo que corresponde a la alarma elegida
    datos.flujo_mlh = datos.flujo_objetivo_mlh;
    datos.segundos_sin_gotas = 0;
    datos.bateria_pct = 70;
    datos.angulo_grados = 1.5f;
    datos.volumen_ml = 250;
    switch (datos.alarma) {
    case UI_ALARMA_OCLUSION:
        datos.flujo_mlh = 0;
        datos.segundos_sin_gotas = datos.umbral_sin_gotas_s;
        break;
    case UI_ALARMA_DESVIACION_FLUJO:
        datos.flujo_mlh = datos.flujo_objetivo_mlh * 5 / 4;   // 25 % por encima del objetivo
        break;
    case UI_ALARMA_FIN_SACHET:
        datos.flujo_mlh = 0;
        datos.volumen_ml = datos.volumen_total_ml;
        break;
    case UI_ALARMA_BATERIA_BAJA:
        datos.bateria_pct = 8;
        break;
    case UI_ALARMA_COLOCACION:
        datos.angulo_grados = 8.0f;
        break;
    default:
        break;
    }
    datos.colocacion_ok = fabsf(datos.angulo_grados) <= TOLERANCIA_GRADOS;
    ESP_LOGI(TAG, "alarma de prueba: %d", datos.alarma);
}
#endif

/*==================[external functions definition]==========================*/
void app_main(void) {
    UI_Init();
    encoder_ok = Encoder_Init();

    // "Encendido": la pantalla queda apagada hasta mantener el botón apretado
    ESP_LOGI(TAG, "Mantener el botón %d ms para encender", ENCENDIDO_MS);
    Encoder_EsperarMantenido(ENCENDIDO_MS);

    AutotestInicio();
    UI_SetChequeos(chequeos, UI_CHEQUEOS_MAX);
    DatosIniciales();
#if PROBAR_ALARMAS
    // Se saltea el autotest, la colocación y la configuración: directo al Monitoreo con la infusión en curso
    datos.infusion_configurada = true;
    datos.volumen_ml = 250;
    datos.flujo_mlh = datos.flujo_objetivo_mlh;
    UI_Ir(UI_PANTALLA_MONITOREO, &datos);
#else
    UI_Ir(UI_PANTALLA_AUTOTEST, &datos);
#endif

    int64_t ultimo_tick_us = esp_timer_get_time();
    encoder_evento_t ev;
    while (1) {
        // Espera un evento hasta que toque el próximo tick
        int64_t resta_ms = PERIODO_TICK_MS - (esp_timer_get_time() - ultimo_tick_us) / 1000;
        if (resta_ms > 0 && Encoder_Leer(&ev, pdMS_TO_TICKS(resta_ms))) {
#if PROBAR_ALARMAS
            ui_pantalla_t pantalla = UI_PantallaActual();
            bool navegando_alarmas = pantalla == UI_PANTALLA_MONITOREO || pantalla == UI_PANTALLA_ALARMA ||
                                     pantalla == UI_PANTALLA_COLOCACION;
            if (navegando_alarmas && (ev == ENC_GIRO_HORARIO || ev == ENC_GIRO_ANTIHORARIO)) {
                CambiarAlarmaDePrueba(ev == ENC_GIRO_HORARIO ? 1 : -1);
                UI_Actualizar(&datos);
            } else {
                UI_Evento(ev, &datos);
            }
#else
            UI_Evento(ev, &datos);
#endif
        }

        if ((esp_timer_get_time() - ultimo_tick_us) / 1000 >= PERIODO_TICK_MS) {
            ultimo_tick_us = esp_timer_get_time();
            SimularSensado();
            UI_Actualizar(&datos);
        }

        if (UI_ApagarPedido()) {
            // Apagado: pantalla apagada hasta que se mantenga el botón de nuevo, y se reinicia
            UI_PantallaApagada();
            Encoder_EsperarMantenido(ENCENDIDO_MS);
            esp_restart();
        }
    }
}
/*==================[end of file]============================================*/
