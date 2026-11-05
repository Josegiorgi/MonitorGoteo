/*! @mainpage Test batería
 *
 * @section genDesc General Description
 *
 * Prueba de la medición de la batería (divisor resistivo + ADC) y de sus alarmas, sin gotas ni las
 * demás alarmas. Al encender (y también si se reinicia) la pantalla queda apagada hasta que se
 * mantiene apretado el click del encoder 3 s, como en el equipo; después arranca directo en la
 * pantalla de Monitoreo, y la carga que muestra ("BAT: N%") es la que se mide en el pin del
 * divisor. Se conecta una fuente variable en lugar de la batería y se la baja de a poco: al llegar
 * a cada escalón el equipo avisa.
 *
 * Usa la lógica real del componente alarmas (solo la parte de batería: en_monitoreo en false, así
 * no se evalúa ninguna otra), los mismos sonidos del buzzer y la misma pantalla de alarma.
 *
 * Escalones (de la especificación de alarmas) y tensión de la batería con la curva de bateria.c
 * (Li-ion/Li-Po de 1 celda, sin carga):
 *
 * | Escalón        | Carga  | Entra al bajar a | Se apaga al subir a | Qué hace                                    |
 * |:---------------|:------:|:----------------:|:-------------------:|:--------------------------------------------|
 * | Aviso          | <= 20 %| 3.731 V          | 3.740 V             | aviso en blanco abajo: "! AVISO BATERIA 20%"|
 * | Alarma media   | <= 10 %| 3.691 V          | 3.700 V             | pantalla "BATERIA BAJA" y un pitido largo   |
 * | Alarma alta    | <= 5 % | 3.617 V          | 3.650 V             | pantalla "BATERIA BAJA" y dos pitidos largos|
 *
 * Los escalones tienen histéresis de 2 puntos porcentuales: una vez dentro, no salen hasta que la
 * carga supere el umbral + 2 %. Un click silencia la alarma que suena (la alta se re-arma a los
 * REARME_PRUEBA_S segundos si la condición sigue).
 *
 * **Girar el encoder** cambia entre el Monitoreo y una página con la tensión: "Vbat" (la de la
 * batería, ya multiplicada por el divisor), "Vpin" (la que llega al pin del ADC), la carga y el
 * escalón. Sirve para comprobar el divisor comparando con un tester, también sin USB. Cuando
 * suena una alarma, la pantalla de alarma tiene prioridad sobre la página de tensión.
 *
 * Por consola (con USB), una vez por segundo: lo mismo, más una línea cada vez que cambia un escalón.
 *
 * @note Conexión: positivo de la fuente al divisor (10 kΩ + 10 kΩ), punto medio al pin definido en
 * bateria.h (BATERIA_CANAL_ADC, GPIO_4) y **GND de la fuente unido al GND de la placa**. No pasar de
 * ~5 V en la fuente: el ADC vería la mitad (2.5 V), el límite de la zona precisa. Sin batería
 * conectada (menos de 2.5 V) la medición no vale y no se da ninguna alarma.
 *
 * @note Si la fuente también va al pad BAT de la placa (para que funcione con "batería") y a la vez
 * hay USB conectado, el cargador de la placa puede empujar la tensión de ese punto hacia 4.2 V y la
 * fuente no la puede bajar (no absorbe corriente): se vería la tensión clavada y el ADC no
 * seguiría a la fuente. Para esta prueba conviene alimentar la placa solo por USB y usar la fuente
 * únicamente en el divisor, o desconectar el USB y mirar la pantalla.
 *
 * @note Si el valor que informa el equipo no coincide con el tester, ajustar BATERIA_AJUSTE_PERMIL en
 * bateria.h (tolerancia de las resistencias) y, si hace falta, poner un capacitor de 100 nF entre el
 * pin del ADC y GND (la fuente de 5 kΩ del divisor es alta para el ADC).
 *
 * @section hardConn Hardware Connection
 *
 * |   Dispositivo                    |   ESP32-C3          |
 * |:--------------------------------:|:-------------------:|
 * | Punto medio del divisor (batería)| GPIO_4 (ADC1_CH4)   |
 * | OLED SCL                         | GPIO_7              |
 * | OLED SDA                         | GPIO_21             |
 * | Encoder CLK                      | GPIO_9              |
 * | Encoder DT                       | GPIO_8              |
 * | Encoder pulsador (SW)            | GPIO_20             |
 * | Buzzer (pasivo)                  | GPIO_10             |
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 05/11/2026 | Document creation                              |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "encoder.h"
#include "ui.h"
#include "display.h"
#include "alarmas.h"
#include "buzzer_alarm.h"
#include "bateria.h"

/*==================[macros and definitions]=================================*/
#define PERIODO_TICK_MS      1000   /*!< Cada cuánto se evalúan las alarmas y se actualiza la pantalla */
#define ESPERA_MAX_MS        100    /*!< Máximo que se espera un evento del encoder: también marca el ritmo de las lecturas de la batería */
#define REARME_PRUEBA_S      20     /*!< Segundos hasta que la alarma alta silenciada se re-arma (en el equipo son 120) */
#define ENCENDIDO_MS         3000   /*!< Tiempo que hay que mantener apretado el click para encender (igual que monitor_goteo) */

static const char *TAG = "test_bateria";

/*==================[internal data definition]===============================*/
static ui_datos_t datos;
static bool pagina_voltaje = false;   /*!< true: se muestra la página con la tensión en vez del Monitoreo (se cambia girando el encoder) */

/*==================[internal functions definition]==========================*/
static void DatosIniciales(void) {
    datos = (ui_datos_t){
        .factor_goteo = 20,
        .volumen_total_ml = 500,
        .duracion_min = 8 * 60,
        .flujo_objetivo_mlh = 62,
        .umbral_sin_gotas_s = 15,
        .bateria_pct = 100,
        .colocacion_ok = true,
        .tolerancia_grados = 8.0f,
        .infusion_configurada = true,    // para que la interfaz muestre las pantallas de alarma
    };
}

/** Alarma de la interfaz que corresponde a un evento de batería */
static ui_alarma_t AlarmaDeInterfaz(int evento) {
    switch (evento) {
    case ALARMA_BATERIA_CRITICA:
    case ALARMA_BATERIA_BAJA:   return UI_ALARMA_BATERIA_BAJA;   // la interfaz tiene una sola pantalla para las dos
    default:                    return UI_ALARMA_NINGUNA;
    }
}

/** Pasa a la interfaz la alarma que corresponde mostrar: la de mayor prioridad sin silenciar o, si
 * no hay, la silenciada (queda el aviso abajo en el Monitoreo). */
static void CargarAlarmaEnInterfaz(void) {
    datos.aviso_bateria = (Alarmas_Activas() & ALARMA_BIT(AVISO_BATERIA)) != 0;   // <= 20 %: aviso en blanco abajo, sin sonido
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

/** Texto del escalón de batería activo */
static const char *Escalon(void) {
    uint32_t activas = Alarmas_Activas();
    if (activas & ALARMA_BIT(ALARMA_BATERIA_CRITICA)) {
        return "ALARMA ALTA (<=5%)";
    }
    if (activas & ALARMA_BIT(ALARMA_BATERIA_BAJA)) {
        return "ALARMA MEDIA (<=10%)";
    }
    if (activas & ALARMA_BIT(AVISO_BATERIA)) {
        return "AVISO (<=20%)";
    }
    return "ok";
}

/** Página de prueba con la tensión: así se puede comprobar el divisor sin consola (sin USB). */
static void DibujarPaginaVoltaje(bool valida) {
    u8g2_t *g = Display_Handle();
    uint16_t pin_mv = Bateria_PinMv();
    uint16_t bat_mv = Bateria_Mv();
    bool silenciada = (Alarmas_Silenciadas() & Alarmas_Activas() & ALARMAS_SONORAS_MASK) != 0;

    Display_Clear();
    u8g2_SetDrawColor(g, 1);
    u8g2_SetFont(g, u8g2_font_6x10_tf);
    Display_Printf(0, 10, "Vbat %u.%03u V", bat_mv / 1000, bat_mv % 1000);
    Display_Printf(0, 22, "Vpin %u.%03u V", pin_mv / 1000, pin_mv % 1000);
    if (valida) {
        Display_Printf(0, 34, "Carga %u %%", datos.bateria_pct);
        Display_Printf(0, 46, "%s%s", Escalon(), silenciada ? " (silenc.)" : "");
    } else {
        Display_Printf(0, 34, "SIN BATERIA (<%d mV)", BATERIA_MV_MINIMO_VALIDO);
    }
    Display_Printf(0, 62, "Girar: volver");
    Display_Flush();
}

/** Dibuja lo que corresponde: la pantalla de alarma si hay una sonando; si no, la página de tensión o el Monitoreo */
static void RedibujarPantalla(bool valida) {
    bool alarma_a_la_vista = datos.alarma != UI_ALARMA_NINGUNA && !datos.alarma_silenciada;
    if (pagina_voltaje && !alarma_a_la_vista && UI_PantallaActual() == UI_PANTALLA_MONITOREO) {
        DibujarPaginaVoltaje(valida);
    } else {
        UI_Actualizar(&datos);
    }
}

/** Se llama cada vez que un escalón de batería se activa o se apaga: queda en la consola con su hora */
static void EventoAlarma(alarma_evento_t evento, bool activo, int64_t ahora_us) {
    switch (evento) {
    case AVISO_BATERIA:
        ESP_LOGW(TAG, ">>> AVISO de bateria (<=20%%) %s", activo ? "ACTIVADO" : "apagado");
        break;
    case ALARMA_BATERIA_BAJA:
        ESP_LOGW(TAG, ">>> ALARMA MEDIA de bateria (<=10%%) %s", activo ? "ACTIVADA" : "apagada");
        break;
    case ALARMA_BATERIA_CRITICA:
        ESP_LOGW(TAG, ">>> ALARMA ALTA de bateria (<=5%%) %s", activo ? "ACTIVADA" : "apagada");
        break;
    default:
        break;
    }
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    bool oled_ok = UI_Init();
    if (!oled_ok) {
        ESP_LOGE(TAG, "la OLED no responde (se sigue solo con la consola)");
    }
    bool encoder_ok = Encoder_Init();
    BuzzerAlarm_Init(BUZZER_ALARM_GPIO);
    Bateria_Init();

    static const alarmas_tiempos_t tiempos = {
        .desv_estabilizacion_s = 120, .desv_leve_persist_s = 300, .desv_fuerte_persist_s = 120, .rearme_s = REARME_PRUEBA_S,
    };
    Alarmas_SetTiempos(&tiempos);
    Alarmas_SetCallback(EventoAlarma);
    Alarmas_Iniciar(20, 500, 8 * 60, esp_timer_get_time());   // la configuración no importa: solo se evalúa la batería

    // "Encendido": la pantalla queda apagada hasta mantener el click apretado, como en monitor_goteo.
    // Si el equipo se reinicia (por ejemplo porque la tensión bajó demasiado), vuelve a pasar por acá.
    ESP_LOGI(TAG, "Mantener el click %d ms para encender", ENCENDIDO_MS);
    if (encoder_ok) {
        Encoder_EsperarMantenido(ENCENDIDO_MS);
    }

    DatosIniciales();
    if (oled_ok) {
        UI_Ir(UI_PANTALLA_MONITOREO, &datos);
    }
    ESP_LOGI(TAG, "Prueba de bateria. Encoder: %s. Divisor %d + %d ohm en GPIO_4. Re-armado de la alarma alta: %d s",
             encoder_ok ? "OK" : "ERROR", BATERIA_R_ARRIBA_OHM, BATERIA_R_ABAJO_OHM, REARME_PRUEBA_S);

    int64_t ultimo_tick_us = esp_timer_get_time();
    encoder_evento_t ev;
    while (1) {
        Bateria_Muestrear(BuzzerAlarm_Sonando());   // mientras suena el buzzer no se mide: sus picos de consumo bajan la tensión

        if (Encoder_Leer(&ev, pdMS_TO_TICKS(ESPERA_MAX_MS)) && oled_ok) {
            if (ev == ENC_GIRO_HORARIO || ev == ENC_GIRO_ANTIHORARIO) {
                pagina_voltaje = !pagina_voltaje;   // girar: cambia entre el Monitoreo y la página de tensión
                RedibujarPantalla(Bateria_Valida());
            } else {
                bool silenciada_antes = datos.alarma_silenciada;
                UI_Evento(ev, &datos);
                if (!silenciada_antes && datos.alarma_silenciada) {   // el click silenció la alarma que sonaba
                    Alarmas_Silenciar(esp_timer_get_time());
                    CargarAlarmaEnInterfaz();
                    AlarmasSonido_Actualizar();
                    RedibujarPantalla(Bateria_Valida());
                }
            }
        }

        int64_t ahora = esp_timer_get_time();
        if ((ahora - ultimo_tick_us) / 1000 >= PERIODO_TICK_MS) {
            ultimo_tick_us = ahora;

            bool valida = Bateria_Valida();
            datos.bateria_pct = valida ? Bateria_Pct() : 100;   // sin batería conectada no se da ninguna alarma
            datos.tiempo_s++;                                   // para ver que la pantalla sigue viva

            alarmas_entrada_t entrada = {
                .en_monitoreo = false,      // solo se evalúa la batería
                .angulo_grados = 0.0f,
                .tolerancia_grados = datos.tolerancia_grados,
                .base_optica_ok = true,
                .bateria_pct = datos.bateria_pct,
                .cargador = false,
            };
            Alarmas_Actualizar(&entrada, ahora);
            CargarAlarmaEnInterfaz();
            AlarmasSonido_Actualizar();
            if (oled_ok) {
                RedibujarPantalla(valida);
            }

            if (valida) {
                ESP_LOGI(TAG, "Vpin=%u mV  Vbat=%u mV  carga=%u %%  escalon: %s",
                         Bateria_PinMv(), Bateria_Mv(), datos.bateria_pct, Escalon());
            } else {
                ESP_LOGW(TAG, "Vpin=%u mV  Vbat=%u mV: sin bateria conectada (menos de %d mV) o sin lecturas todavia",
                         Bateria_PinMv(), Bateria_Mv(), BATERIA_MV_MINIMO_VALIDO);
            }
        }
    }
}
/*==================[end of file]============================================*/
