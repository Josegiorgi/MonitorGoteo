/*! @mainpage Test display
 *
 * @section genDesc General Description
 *
 * Prueba la OLED coordinada con el encoder y con el conteo de gotas de los BPW34. No usa el
 * acelerómetro.
 *
 * Flujo:
 * 1. Inicio: la pantalla "Iniciando" con su animación (INICIO_MS), como al encender el equipo.
 * 2. Factor de goteo: pide solo el factor. Girar el encoder lo cambia (10, 15, 20 o 60 gtt/mL) y
 *    el click lo acepta.
 * 3. Monitoreo: muestra el caudal en mL/h, el conteo de gotas, el volumen y los segundos sin gotas.
 *    El caudal sale de las gotas que detecta el algoritmo de correlación cruzada (drop_detection).
 *    Un círculo arriba a la derecha se llena un instante con cada gota.
 * 4. Alarma de fin de sachet (simulada): si pasan más de FIN_SACHET_SIN_GOTAS_S segundos sin que
 *    caiga una gota, la pantalla pasa a la alarma y suena el buzzer.
 *    - 1 click: silencia la alarma (queda en pantalla).
 *    - 2 clicks: la resuelve (vuelve al monitoreo y la cuenta de segundos sin gotas arranca de
 *      cero). Si vuelven a caer gotas, la alarma también se apaga sola.
 * Un click largo en el monitoreo vuelve a pedir el factor de goteo (y reinicia el conteo).
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
 * @note El LED infrarrojo va siempre encendido por hardware. El tubo tiene que estar en la cámara
 * y sin gotas al encender, porque el sensor mide su nivel base al arrancar.
 *
 * @note El controlador de la OLED (SSD1306, SH1106 o SSD1315) y su dirección se eligen por
 * menuconfig, en "Display (OLED I2C 128x64)". Por defecto SSD1315 en 0x3C.
 *
 * @note La consola va por USB Serial/JTAG (sdkconfig.defaults).
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation		                         |
 * | 29/10/2026 | Sin ángulo: OLED coordinada con el encoder y el conteo de gotas, y alarma simulada de fin de sachet |
 * | 29/10/2026 | Flujo completo: inicio, pedido del factor de goteo, monitoreo en mL/h y alarma (1 click silencia, 2 resuelven) |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_mcu.h"
#include "display.h"
#include "encoder.h"
#include "ui.h"
#include "goteo.h"
#include "drop_detection.h"
#include "buzzer_alarm.h"

/*==================[macros and definitions]=================================*/
#define I2C_FREQ_HZ             100000
#define INICIO_MS               3000    /*!< Duración de la pantalla de inicio */
#define PERIODO_ANIMACION_MS    42      /*!< Cada cuánto avanza un cuadro de la animación del inicio */
#define FIN_SACHET_SIN_GOTAS_S  40      /*!< Segundos sin gotas a partir de los cuales se simula el fin de sachet */
#define REFRESCO_MS             200     /*!< Cada cuánto se redibuja la pantalla */
#define INDICADOR_GOTA_MS       300     /*!< Cuánto queda encendido el indicador después de cada gota */
#define COLA_GOTAS_LARGO        32
#define ESPERA_EVENTO_MS        50      /*!< Máximo que se espera un evento del encoder antes de mirar las gotas */

typedef enum {
    ESTADO_FACTOR,
    ESTADO_MONITOREO,
} estado_t;

static const char *TAG = "test_display";

/*==================[internal data definition]===============================*/
static const uint16_t factores[] = {10, 15, 20, 60};
#define N_FACTORES (sizeof(factores) / sizeof(factores[0]))

static QueueHandle_t cola_gotas = NULL;
static uint8_t i_factor = 2;            /* 20 gtt/mL por defecto */
static estado_t estado = ESTADO_FACTOR;
static bool alarma_activa = false;
static bool alarma_silenciada = false;
static bool suena = false;              /* estado del buzzer, para arrancarlo o cortarlo solo al cambiar */

/*==================[internal functions definition]==========================*/
/** Llamada por drop_detection en la tarea del sensor: solo guarda la hora de la gota. */
static void AvisoGota(int64_t ahora_us) {
    xQueueSend(cola_gotas, &ahora_us, 0);
}

/** Texto centrado horizontalmente con la fuente actual. */
static void TextoCentrado(int y, const char *texto) {
    u8g2_t *g = Display_Handle();
    u8g2_DrawStr(g, (128 - u8g2_GetStrWidth(g, texto)) / 2, y, texto);
}

static void PantallaFactor(void) {
    u8g2_t *g = Display_Handle();
    char valor[16];
    snprintf(valor, sizeof(valor), "%u gtt/mL", factores[i_factor]);

    u8g2_SetFont(g, u8g2_font_6x10_tf);
    TextoCentrado(12, "Factor de goteo");
    u8g2_SetFont(g, u8g2_font_ncenB14_tr);
    TextoCentrado(38, valor);
    u8g2_SetFont(g, u8g2_font_6x10_tf);
    TextoCentrado(54, "Girar: cambiar");
    TextoCentrado(64, "Click: aceptar");
}

static void PantallaMonitoreo(bool indicador, int64_t ahora) {
    u8g2_t *g = Display_Handle();
    char texto[24];

    snprintf(texto, sizeof(texto), "%u mL/h", Goteo_CaudalMlh(ahora));
    u8g2_SetFont(g, u8g2_font_ncenB14_tr);
    TextoCentrado(22, texto);

    u8g2_SetFont(g, u8g2_font_6x10_tf);
    Display_Printf(0, 38, "Gotas %5lu  %3u mL", (unsigned long)Goteo_Gotas(), Goteo_VolumenMl());
    Display_Printf(0, 52, "FG %2u", factores[i_factor]);
    Display_Printf(0, 63, "Sin gotas %3u s", Goteo_SegundosSinGotas(ahora));
    if (indicador) {
        u8g2_DrawDisc(g, 118, 56, 7, U8G2_DRAW_ALL);   // se enciende un instante con cada gota
    } else {
        u8g2_DrawCircle(g, 118, 56, 7, U8G2_DRAW_ALL);
    }
}

/** Alarma de fin de sachet (fondo blanco con texto negro). */
static void PantallaAlarma(int64_t ahora) {
    u8g2_t *g = Display_Handle();
    u8g2_SetFont(g, u8g2_font_6x10_tf);
    u8g2_SetDrawColor(g, 1);
    u8g2_DrawBox(g, 0, 0, 128, 64);
    u8g2_SetDrawColor(g, 0);
    TextoCentrado(12, "ALARMA ALTA");
    TextoCentrado(26, "FIN DE SACHET");
    Display_Printf(8, 40, "Sin gotas %3u s", Goteo_SegundosSinGotas(ahora));
    TextoCentrado(53, alarma_silenciada ? "(silenciada)" : "1 click: silenciar");
    TextoCentrado(63, "2 clicks: resolver");
    u8g2_SetDrawColor(g, 1);
}

/** Pone el buzzer y la detección de doble click en línea con el estado de la alarma. */
static void SincronizarAlarma(void) {
    bool debe_sonar = alarma_activa && !alarma_silenciada;
    if (debe_sonar != suena) {
        suena = debe_sonar;
        if (suena) {
            BuzzerAlarm_Start(BUZZER_ALARM_FIN_SACHET);
        } else {
            BuzzerAlarm_Stop();
        }
    }
    Encoder_SetDobleClick(alarma_activa);   // el doble click solo se usa con la alarma en pantalla
}

static void IniciarConteo(void) {
    Goteo_Iniciar(factores[i_factor], esp_timer_get_time());
    alarma_activa = false;
    alarma_silenciada = false;
    SincronizarAlarma();
}

/** Pantalla de inicio con su animación, como al encender el equipo. */
static void Inicio(void) {
    int64_t inicio_us = esp_timer_get_time();
    uint32_t t_ms;
    do {
        t_ms = (esp_timer_get_time() - inicio_us) / 1000;
        UI_PantallaInicio(t_ms / PERIODO_ANIMACION_MS);
    } while (t_ms < INICIO_MS);
}

/*==================[external functions definition]==========================*/
void app_main(void)
{
    if (!I2C_initialize(I2C_FREQ_HZ)) {
        ESP_LOGE(TAG, "no se pudo inicializar el bus I2C");
        return;
    }
    bool display_ok = Display_Init();
    ESP_LOGI(TAG, "OLED: %s", display_ok ? "OK" : "no responde (se sigue sin pantalla)");
    bool encoder_ok = Encoder_Init();
    ESP_LOGI(TAG, "Encoder: %s", encoder_ok ? "OK" : "error");
    BuzzerAlarm_Init(BUZZER_ALARM_GPIO);
    cola_gotas = xQueueCreate(COLA_GOTAS_LARGO, sizeof(int64_t));
    bool sensor_ok = DropDetection_Init(AvisoGota);
    ESP_LOGI(TAG, "Sensor de gotas: %s", sensor_ok ? "OK" : "error");

    if (display_ok) {
        Inicio();
    }

    int64_t ultima_gota_us = 0;       /* hora de la última gota, para el indicador */
    int64_t ultimo_dibujo_us = 0;
    encoder_evento_t ev;

    while (1) {
        bool redibujar = false;

        /* Encoder */
        if (Encoder_Leer(&ev, pdMS_TO_TICKS(ESPERA_EVENTO_MS))) {
            redibujar = true;
            if (estado == ESTADO_FACTOR) {
                if (ev == ENC_GIRO_HORARIO || ev == ENC_GIRO_ANTIHORARIO) {
                    i_factor = (i_factor + N_FACTORES + (ev == ENC_GIRO_HORARIO ? 1 : -1)) % N_FACTORES;
                } else if (ev == ENC_CLICK) {
                    estado = ESTADO_MONITOREO;
                    IniciarConteo();
                    ultima_gota_us = 0;
                    ESP_LOGI(TAG, "factor de goteo aceptado: %u gtt/mL", factores[i_factor]);
                }
            } else if (alarma_activa) {
                if (ev == ENC_CLICK && !alarma_silenciada) {
                    alarma_silenciada = true;
                    SincronizarAlarma();
                    ESP_LOGI(TAG, "alarma silenciada");
                } else if (ev == ENC_DOBLE_CLICK) {
                    // Resuelta: la cuenta de segundos sin gotas vuelve a empezar de cero
                    Goteo_ReiniciarTiempo(esp_timer_get_time());
                    alarma_activa = false;
                    alarma_silenciada = false;
                    SincronizarAlarma();
                    ESP_LOGI(TAG, "alarma resuelta");
                }
            } else if (ev == ENC_CLICK_LARGO) {
                estado = ESTADO_FACTOR;   // vuelve a pedir el factor
                ESP_LOGI(TAG, "se vuelve a pedir el factor de goteo");
            }
        }

        /* Gotas: en el monitoreo se cuentan; en el pedido del factor se descartan */
        int64_t t_us;
        while (xQueueReceive(cola_gotas, &t_us, 0) == pdTRUE) {
            if (estado == ESTADO_MONITOREO) {
                Goteo_Gota(t_us);
                ultima_gota_us = t_us;
                int canal;
                float pico = DropDetection_UltimoPico(&canal);
                ESP_LOGI(TAG, "gota %lu (canal %d, pico %.0f mV; umbral 160/140)", (unsigned long)Goteo_Gotas(), canal, pico);
                if (alarma_activa) {   // volvieron las gotas: la alarma se apaga sola
                    alarma_activa = false;
                    alarma_silenciada = false;
                    SincronizarAlarma();
                    ESP_LOGI(TAG, "alarma apagada (volvieron las gotas)");
                }
                redibujar = true;
            }
        }

        /* Alarma simulada: más de FIN_SACHET_SIN_GOTAS_S segundos sin que caiga ninguna gota */
        int64_t ahora = esp_timer_get_time();
        if (estado == ESTADO_MONITOREO && !alarma_activa && Goteo_SegundosSinGotas(ahora) > FIN_SACHET_SIN_GOTAS_S) {
            alarma_activa = true;
            alarma_silenciada = false;
            SincronizarAlarma();
            ESP_LOGI(TAG, "ALARMA fin de sachet (mas de %d s sin gotas)", FIN_SACHET_SIN_GOTAS_S);
            redibujar = true;
        }

        /* Pantalla */
        if (display_ok && (redibujar || (ahora - ultimo_dibujo_us) / 1000 >= REFRESCO_MS)) {
            Display_Clear();
            if (estado == ESTADO_FACTOR) {
                PantallaFactor();
            } else if (alarma_activa) {
                PantallaAlarma(ahora);
            } else {
                PantallaMonitoreo(ultima_gota_us != 0 && (ahora - ultima_gota_us) / 1000 < INDICADOR_GOTA_MS, ahora);
            }
            Display_Flush();
            ultimo_dibujo_us = esp_timer_get_time();
        }
    }
}

/*==================[end of file]============================================*/
