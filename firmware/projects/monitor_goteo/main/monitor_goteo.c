/*! @mainpage Monitor de goteo
 *
 * \section genDesc General Description
 *
 * Firmware del monitor de goteo con interfaz (OLED + encoder), sensado de gotas con los BPW34 y
 * alarmas con aviso sonoro. Por ahora no mide el ángulo (ADXL345): la colocación no se evalúa.
 *
 * Al arrancar hay que mantener apretado el botón 3 s ("encendido"). Sigue la pantalla de inicio
 * (autotest) y después Autotest -> Colocación -> Config -> Resumen -> Ajuste de goteo ->
 * Monitoreo. Con la infusión en curso, el usuario solo puede silenciar la alarma con un click; el
 * doble click abre el Acceso técnico (código 1234).
 *
 * - Las gotas las detecta drop_detection (BPW34) y llegan a esta aplicación por una cola con la
 *   hora de cada una. Se procesan enseguida: cada gota se cuenta, actualiza el volumen y el caudal
 *   en pantalla (se redibuja como mucho cada REFRESCO_MIN_MS) y queda en la consola.
 * - Una vez por segundo se evalúan las alarmas (componente alarmas, según la especificación de
 *   alarmas), se pasa la de mayor prioridad a la interfaz y se hace sonar en el buzzer. Al pulsar
 *   en una alarma se silencia: deja de sonar, aparece la siguiente (si hay) y las de prioridad alta
 *   se re-arman a los 2 min si la condición sigue.
 *
 * Para probar las alarmas:
 * - Flujo detenido: tapar el sensor o cortar el goteo. Suena a los T_ocl (mínimo 15 s).
 * - Fin de sachet: con el volumen en 90 % o más, cortar el goteo.
 * - Desviación: cambiar el goteo con el clamp. Aviso a los 5 min con más de 10 % y alarma a los
 *   2 min con más de 25 %. No se evalúa en los primeros 2 min del Monitoreo.
 * - Batería: con BATERIA_SIMULADA en 1, girar el encoder en la pantalla de Monitoreo baja o sube
 *   la carga simulada de a 5 %. 20 % aviso, 10 % alarma media, 5 % alarma alta.
 *
 * Diferencias con la especificación que quedan para cuando se adapte ui.c: la interfaz sigue
 * usando un único ui_alarma_t, así que VOLUMEN_COMPLETO se muestra como "fin de sachet" y, en esa
 * pantalla, el click da por terminada la infusión; la colocación sigue pidiendo click; y el Resumen
 * todavía no impide iniciar con menos de 2 gotas/min.
 *
 * @section hardConn Hardware Connection
 *
 * |   Dispositivo                    |   ESP32-C3          |
 * |:--------------------------------:|:-------------------:|
 * | Salida amplificador BPW34 n°1    | GPIO_2 (ADC1_CH2)   |
 * | Salida amplificador BPW34 n°2    | GPIO_3 (ADC1_CH3)   |
 * | OLED SCL                         | GPIO_7              |
 * | OLED SDA                         | GPIO_21             |
 * | Encoder CLK                      | GPIO_9              |
 * | Encoder DT                       | GPIO_8              |
 * | Encoder pulsador (SW)            | GPIO_20             |
 * | Buzzer (pasivo)                  | GPIO_10             |
 *
 * @note El LED infrarrojo va siempre encendido por hardware. El tubo de goteo tiene que estar en
 * la cámara y sin gotas al encender, porque el sensor mide su nivel base al arrancar.
 *
 * @note La consola va por USB Serial/JTAG (sdkconfig.defaults): GPIO20/21 son RX/TX de UART0.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation: interfaz de prueba_interfaz con gotas reales, alarmas y buzzer |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "encoder.h"
#include "ui.h"
#include "goteo.h"
#include "alarmas.h"
#include "buzzer_alarm.h"
#include "drop_detection.h"

/*==================[macros and definitions]=================================*/
#define ENCENDIDO_MS          3000  /*!< Tiempo que hay que mantener apretado el botón para encender */
#define DURACION_INICIO_MS    5000  /*!< Duración mínima de la pantalla de inicio (autotest) */
#define PERIODO_CHEQUEO_MS    1000  /*!< Cada cuánto se hace o reintenta un chequeo del autotest */
#define PERIODO_ANIMACION_MS  42    /*!< Cada cuánto avanza un cuadro de la animación del inicio */
#define PERIODO_TICK_MS       1000  /*!< Cada cuánto se evalúan las alarmas y se actualiza la interfaz */
#define ESPERA_MAX_MS         100   /*!< Máximo que se espera un evento del encoder antes de mirar la cola de gotas */
#define REFRESCO_MIN_MS       250   /*!< Mínimo entre dos redibujados de pantalla provocados por gotas */
#define COLA_GOTAS_LARGO      32

#define TOLERANCIA_GRADOS     2.5f  /*!< Provisorio: sin ADXL345 el ángulo se da por correcto */
#define BATERIA_SIMULADA      1     /*!< 1: girar el encoder en el Monitoreo sube/baja la batería simulada de a 5 % */
#define BATERIA_PASO_PCT      5

static const char *TAG = "monitor_goteo";

/*==================[internal data definition]===============================*/
static ui_datos_t datos;
static ui_chequeo_t chequeos[UI_CHEQUEOS_MAX] = {
    {"OLED", UI_CHEQUEO_PENDIENTE},
    {"ENC", UI_CHEQUEO_PENDIENTE},
    {"ACEL", UI_CHEQUEO_PENDIENTE},
    {"FOTO", UI_CHEQUEO_PENDIENTE},
};
static bool oled_ok = false;
static bool encoder_ok = false;
static bool sensor_ok = false;
static bool midiendo = false;          /*!< true desde que empieza el Ajuste de goteo hasta que termina la infusión */
static QueueHandle_t cola_gotas = NULL;   /*!< Hora de cada gota detectada (de la tarea del sensor al main) */
static int64_t ultimo_refresco_us = 0;

/*==================[internal functions definition]==========================*/
static void DatosIniciales(void) {
    datos = (ui_datos_t){
        .factor_goteo = 20,
        .volumen_total_ml = 500,
        .duracion_min = 8 * 60,
        .flujo_objetivo_mlh = (500 * 60 + 240) / (8 * 60),   // volumen / tiempo; después lo recalcula la interfaz al cambiar Config
        .umbral_sin_gotas_s = 15,
        .bateria_pct = 100,
        .angulo_grados = 0.0f,
        .colocacion_ok = true,
        .tolerancia_grados = TOLERANCIA_GRADOS,
    };
}

/* Chequeos del autotest. Cada uno devuelve true si el componente responde. */
static bool ChequearOled(void) {
    return oled_ok;
}

static bool ChequearEncoder(void) {
    return encoder_ok;
}

static bool ChequearAcelerometro(void) {
    return true;   // TODO: se mide el ángulo con el ADXL345 (leer DEVID 0xE5)
}

static bool ChequearFotodiodos(void) {
    return sensor_ok;   // TODO: verificar además que el nivel base de los dos canales esté en rango
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
 * ERR y se reintenta cada segundo. */
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
        UI_PantallaInicio(t_ms / PERIODO_ANIMACION_MS);
    } while (t_ms < DURACION_INICIO_MS || !TodosOk());
}

/** Llamada por drop_detection en la tarea del sensor: solo guarda la hora de la gota. */
static void AvisoGota(int64_t ahora_us) {
    xQueueSend(cola_gotas, &ahora_us, 0);   // si la cola está llena se pierde la gota
}

/** Empieza a contar gotas. Se llama al entrar al Ajuste de goteo: desde que se abre el clamp ya
 * pasa líquido al paciente, así que el volumen cuenta desde acá. El tiempo, no (ver Monitoreo). */
static void IniciarMedicion(void) {
    int64_t ahora = esp_timer_get_time();
    Goteo_Iniciar(datos.factor_goteo, ahora);
    Alarmas_Iniciar(datos.factor_goteo, datos.volumen_total_ml, datos.duracion_min, ahora);
    datos.umbral_sin_gotas_s = (uint16_t)Alarmas_TOclusionS();
    midiendo = true;
}

/** Mira si cambió la pantalla por un evento y arranca lo que corresponda. */
static void RevisarTransiciones(ui_pantalla_t antes) {
    ui_pantalla_t ahora = UI_PantallaActual();
    if (antes == UI_PANTALLA_RESUMEN && ahora == UI_PANTALLA_AJUSTE_GOTEO) {
        IniciarMedicion();
    } else if (antes == UI_PANTALLA_AJUSTE_GOTEO && ahora == UI_PANTALLA_MONITOREO) {
        Goteo_ReiniciarTiempo(esp_timer_get_time());   // el tiempo de la infusión arranca recién acá
    }
    if (!datos.infusion_configurada) {
        midiendo = false;
    }
}

/** Saca de la cola las gotas detectadas y las cuenta. Devuelve cuántas eran. */
static int ProcesarGotas(void) {
    int64_t t_us;
    int n = 0;
    while (xQueueReceive(cola_gotas, &t_us, 0) == pdTRUE) {
        if (midiendo) {
            Goteo_Gota(t_us);
            Alarmas_Gota(t_us);
            n++;
            ESP_LOGI(TAG, "gota %lu", (unsigned long)Goteo_Gotas());
        }
    }
    return n;
}

/** Carga en datos lo que calcula el módulo goteo: volumen, caudal, tiempo y segundos sin gotas. */
static void ActualizarMedicion(int64_t ahora) {
    if (!midiendo) {
        return;
    }
    datos.volumen_ml = Goteo_VolumenMl();
    datos.flujo_mlh = Goteo_CaudalMlh(ahora);
    datos.segundos_sin_gotas = Goteo_SegundosSinGotas(ahora);
    datos.tiempo_s = (UI_PantallaActual() == UI_PANTALLA_AJUSTE_GOTEO) ? 0 : Goteo_TiempoS(ahora);
}

/** Alarma de la interfaz que corresponde a un evento del componente alarmas */
static ui_alarma_t AlarmaDeInterfaz(int evento) {
    switch (evento) {
    case ALARMA_FLUJO_DETENIDO:     return UI_ALARMA_OCLUSION;
    case ALARMA_DESVIACION_FUERTE:  return UI_ALARMA_DESVIACION_FLUJO;
    case ALARMA_FIN_SACHET:
    case ALARMA_VOLUMEN_COMPLETO:   return UI_ALARMA_FIN_SACHET;   // la interfaz todavía no tiene pantalla propia para volumen completo
    case ALARMA_BATERIA_CRITICA:
    case ALARMA_BATERIA_BAJA:       return UI_ALARMA_BATERIA_BAJA;
    case ALARMA_COLOCACION:         return UI_ALARMA_COLOCACION;
    default:                        return UI_ALARMA_NINGUNA;
    }
}

/** Pasa a la interfaz la alarma que corresponde mostrar: la de mayor prioridad sin silenciar o,
 * si no hay, la silenciada de mayor prioridad (queda el aviso abajo en el Monitoreo). */
static void CargarAlarmaEnInterfaz(void) {
    int mostrar = Alarmas_Mostrar();
    int pie = Alarmas_Pie();
    if (mostrar >= 0) {
        datos.alarma = AlarmaDeInterfaz(mostrar);
        datos.alarma_silenciada = false;
    } else if (pie >= 0 && pie < AVISO_DESVIACION_LEVE) {
        datos.alarma = AlarmaDeInterfaz(pie);
        datos.alarma_silenciada = true;
    } else {
        datos.alarma = UI_ALARMA_NINGUNA;
        datos.alarma_silenciada = false;
    }
}

/** true si la infusión está en curso y hay que evaluar las alarmas (desde el Monitoreo hasta el fin) */
static bool EnMonitoreo(void) {
    if (!midiendo || !datos.infusion_configurada) {
        return false;
    }
    switch (UI_PantallaActual()) {
    case UI_PANTALLA_MONITOREO:
    case UI_PANTALLA_ALARMA:
    case UI_PANTALLA_ACCESO:
    case UI_PANTALLA_MENU_TECNICO:
    case UI_PANTALLA_APAGAR:
    case UI_PANTALLA_COLOCACION:
        return true;
    default:
        return false;
    }
}

/** Una vez por segundo: evalúa las alarmas, las pasa a la interfaz y las hace sonar. */
static void EvaluarAlarmas(int64_t ahora) {
    if (!midiendo) {
        return;
    }
    alarmas_entrada_t entrada = {
        .en_monitoreo = EnMonitoreo(),
        .en_pausa = datos.en_pausa,
        .angulo_grados = datos.angulo_grados,
        .tolerancia_grados = datos.tolerancia_grados,
        .base_optica_ok = true,          // TODO: nivel de base del fotodiodo en rango
        .bateria_pct = datos.bateria_pct,
        .cargador = false,               // TODO: detección de cargador
    };
    Alarmas_Actualizar(&entrada, ahora);
    CargarAlarmaEnInterfaz();
    AlarmasSonido_Actualizar();
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    oled_ok = UI_Init();
    if (!oled_ok) {
        ESP_LOGE(TAG, "la OLED no responde");
    }
    encoder_ok = Encoder_Init();
    BuzzerAlarm_Init(BUZZER_ALARM_GPIO);
    cola_gotas = xQueueCreate(COLA_GOTAS_LARGO, sizeof(int64_t));
    sensor_ok = DropDetection_Init(AvisoGota);

    // "Encendido": la pantalla queda apagada hasta mantener el botón apretado
    ESP_LOGI(TAG, "Mantener el botón %d ms para encender", ENCENDIDO_MS);
    Encoder_EsperarMantenido(ENCENDIDO_MS);

    AutotestInicio();
    UI_SetChequeos(chequeos, UI_CHEQUEOS_MAX);
    DatosIniciales();
    UI_Ir(UI_PANTALLA_AUTOTEST, &datos);

    int64_t ultimo_tick_us = esp_timer_get_time();
    encoder_evento_t ev;
    while (1) {
        // Espera un evento del encoder, pero no más de ESPERA_MAX_MS para atender las gotas
        int64_t resta_ms = PERIODO_TICK_MS - (esp_timer_get_time() - ultimo_tick_us) / 1000;
        if (resta_ms > ESPERA_MAX_MS) {
            resta_ms = ESPERA_MAX_MS;
        }
        if (resta_ms > 0 && Encoder_Leer(&ev, pdMS_TO_TICKS(resta_ms))) {
            ui_pantalla_t pantalla = UI_PantallaActual();
            bool silenciada_antes = datos.alarma_silenciada;
#if BATERIA_SIMULADA
            if (pantalla == UI_PANTALLA_MONITOREO && (ev == ENC_GIRO_HORARIO || ev == ENC_GIRO_ANTIHORARIO)) {
                int pct = datos.bateria_pct + (ev == ENC_GIRO_HORARIO ? BATERIA_PASO_PCT : -BATERIA_PASO_PCT);
                datos.bateria_pct = (pct < 0) ? 0 : (pct > 100) ? 100 : pct;
                ESP_LOGI(TAG, "bateria simulada: %u %%", datos.bateria_pct);
                ev = 0;   // el evento no llega a la interfaz
            }
#endif
            if (ev != 0) {
                UI_Evento(ev, &datos);
            }
            RevisarTransiciones(pantalla);

            if (!silenciada_antes && datos.alarma_silenciada) {
                Alarmas_Silenciar(esp_timer_get_time());
                CargarAlarmaEnInterfaz();
                AlarmasSonido_Actualizar();
                UI_Actualizar(&datos);
            }
        }

        int64_t ahora = esp_timer_get_time();
        int nuevas = ProcesarGotas();
        if (nuevas > 0 && (ahora - ultimo_refresco_us) / 1000 >= REFRESCO_MIN_MS) {
            // Una gota nueva: se muestra enseguida en el Monitoreo (volumen, caudal) sin esperar al próximo tick
            ui_pantalla_t p = UI_PantallaActual();
            if (p == UI_PANTALLA_MONITOREO || p == UI_PANTALLA_AJUSTE_GOTEO) {
                ActualizarMedicion(ahora);
                UI_Actualizar(&datos);
                ultimo_refresco_us = esp_timer_get_time();
            }
        }

        if ((ahora - ultimo_tick_us) / 1000 >= PERIODO_TICK_MS) {
            ultimo_tick_us = ahora;
            ActualizarMedicion(ahora);
            EvaluarAlarmas(ahora);
            UI_Actualizar(&datos);
            ultimo_refresco_us = esp_timer_get_time();
        }

        if (UI_ApagarPedido()) {
            // Apagado: pantalla apagada hasta que se mantenga el botón de nuevo, y se reinicia
            BuzzerAlarm_Stop();
            UI_PantallaApagada();
            Encoder_EsperarMantenido(ENCENDIDO_MS);
            esp_restart();
        }
    }
}
/*==================[end of file]============================================*/
