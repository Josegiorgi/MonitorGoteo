/*! @mainpage Monitor de goteo
 *
 * \section genDesc General Description
 *
 * Firmware del monitor de goteo con interfaz (OLED + encoder), sensado de gotas con los BPW34 y
 * alarmas con aviso sonoro. Mide el ángulo del equipo con el ADXL345 (componente inclinacion) para
 * la pantalla y la alarma de colocación, y la carga de la batería con un divisor de tensión en
 * GPIO_4 (componente bateria) para las alarmas de batería.
 *
 * Al arrancar hay que mantener apretado el botón 3 s ("encendido"). Sigue la pantalla de inicio,
 * donde se hace el autotest (si un chequeo falla muestra la pantalla Autotest con el error y no deja
 * seguir), y después Clave -> Colocación -> Config -> Resumen -> Ajuste de goteo -> Monitoreo.
 * La clave clínica (UI_CLAVE_CLINICA, provisoria 1234) se pide en el arranque y para entrar a Ajustes.
 * Con la infusión en curso, el usuario solo puede silenciar la alarma con un click; manteniendo el
 * botón 3 s se abre la clave (3 errores seguidos bloquean 60 s) y de ahí Ajustes: Cambiar datos (solo
 * la duración), Detener monitoreo, Apagar y Volver. Al detener queda "Sin infusión activa".
 *
 * - Las gotas las detecta drop_detection (BPW34) y llegan a esta aplicación por una cola con la
 *   hora de cada una. Se procesan enseguida: cada gota se cuenta, actualiza el volumen y el caudal
 *   en pantalla (se redibuja como mucho cada REFRESCO_MIN_MS) y queda en la consola.
 * - Una vez por segundo se evalúan las alarmas (componente alarmas, según la especificación de
 *   alarmas), se pasa la de mayor prioridad a la interfaz y se hace sonar en el buzzer. Al pulsar
 *   en una alarma se silencia: deja de sonar, aparece la siguiente (si hay) y las de prioridad alta
 *   se re-arman a los 2 min si la condición sigue.
 *
 * ESCENARIO (macro de escenarios.h) elige con qué arranca la configuración y si los tiempos de las
 * alarmas se acortan para probar sin esperar minutos. Por defecto es ESCENARIO_NORMAL (valores
 * reales). Los de prueba precargan Factor, Volumen y Tiempo en Config (se aceptan con los clicks de
 * siempre) y acortan la estabilización del desvío a 20 s, su persistencia a 20 s (30 s el aviso) y
 * el re-armado a 20 s; además el goteo sin monitoreo a 20 s (se resuelve a los 15 s sin gotas), la inactividad de
 * la clave, Ajustes y Sin infusión a 20 s, la del Modo Técnico a 30 s, el bloqueo de la clave a 15 s, el recordatorio del
 * Ajuste de goteo a los 30 s (beep cada 10 s), la muestra del registro cada 15 s.
 * Al arrancar, un escenario de prueba lo avisa en el log y al compilar (la
 * batería se prueba aparte, en projects/test_bateria):
 *
 * | Escenario                   | Config                   | Objetivo        | Para probar                                    |
 * |:----------------------------|:-------------------------|:----------------|:-----------------------------------------------|
 * | ESCENARIO_OCLUSION_FIN      | 10 gtt/mL, 50 mL, 10 min | 50 gtt/min      | flujo detenido (cortando el goteo con menos de 90 %), fin de sachet (cortándolo con 90 % o más) y volumen completo (seguir pasando de 100 %): 500 gotas. Clamp cerrado en el Ajuste de goteo: el volumen cuenta desde ahí |
 * | ESCENARIO_DESVIO_COLOCACION | 20 gtt/mL, 500 mL, 8 h   | 20,8 gtt/min    | desvío (con un goteo más rápido hay desvío alto) y colocación juntos, silenciado y prioridad. Para flujo bajo, configurar 1 h 30 min |
 *
 * Para probar las alarmas:
 * - Flujo detenido: tapar el sensor o cortar el goteo. Suena a los T_ocl (mínimo 15 s).
 * - Fin de sachet: con el volumen en 90 % o más, cortar el goteo.
 * - Desviación: cambiar el goteo con el clamp. Aviso a los 5 min con más de 10 % y alarma a los
 *   2 min con más de 30 %. No se evalúa en los primeros 2 min del Monitoreo.
 * - Batería: se mide con el divisor 10k + 10k en GPIO_4 (batería 18650 de 1 celda, sin USB). 20 %
 *   aviso blanco abajo en el Monitoreo, 10 % alarma media, 5 % alarma alta. Sin batería conectada
 *   (menos de 2,5 V) queda en 100 % y sin alarma. Para probarla variando la tensión, usar test_bateria.
 * - Colocación: inclinar el equipo más de TOLERANCIA_GRADOS (hacia cualquier lado) y mantenerlo 5 s.
 *   Si el acelerómetro deja de responder en el uso, también se da la alarma: la colocación no se
 *   puede verificar.
 *
 * Cómo se resuelve cada alarma: un click solo la silencia (también fin de sachet y volumen
 * completo). Flujo detenido y fin de sachet se resuelven solos con 3 gotas seguidas; volumen
 * completo pasa a fin de sachet si se corta el goteo; el desvío, cuando vuelve a menos de 28 %; la
 * colocación, a los 5 s con el ángulo en rango (el click en su pantalla la silencia y vuelve al Monitoreo). La infusión la finaliza la enfermera: mantener
 * el botón 3 s, clave, Ajustes > Detener monitoreo (apaga el sonido y deja pasar a una nueva infusión).
 *
 * Registro pendiente de descarga (diseño del 10/11/2026, ver el README.md del proyecto): al terminar la infusión, o al
 * reiniciarse con una infusión sin terminar (que YA NO se retoma: queda interrumpida y suena la alarma EQUIPO_REINICIADO), el
 * minuto sigue contando hasta que la app confirme la descarga con OK (módulo pendiente.c). Mientras tanto la pantalla avisa
 * "Registro pendiente de descarga" (sin sonido) y se pregunta antes de empezar una infusión nueva o de apagar.
 *
 * Diferencia con la especificación que queda: la interfaz sigue usando un único ui_alarma_t, así que VOLUMEN_COMPLETO se
 * muestra como "fin de sachet" y la batería crítica como "batería baja".
 *
 * @section hardConn Hardware Connection
 *
 * |   Dispositivo                    |   ESP32-C3          |
 * |:--------------------------------:|:-------------------:|
 * | Salida amplificador BPW34 n°1    | GPIO_2 (ADC1_CH2)   |
 * | Salida amplificador BPW34 n°2    | GPIO_3 (ADC1_CH3)   |
 * | OLED SCL                         | GPIO_7              |
 * | OLED SDA                         | GPIO_21             |
 * | ADXL345 SCL / SDA                | GPIO_7 / GPIO_21 (mismo bus que la OLED) |
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
 * | 04/11/2026 | Se integra el ADXL345 (componente inclinacion): autotest "ACEL" real, ángulo en la pantalla de colocación y alarma de colocación. Tolerancia de 2,5° a 8° según ../analisis_bpw34 |
 * | 04/11/2026 | BATERIA_SIMULADA pasa a 0: la alarma de batería se prueba aparte, después de probar el resto |
 * | 04/11/2026 | Escenarios de prueba (ESCENARIO): oclusión y fin de sachet, desvío y colocación, y batería, con la configuración precargada y tiempos de alarma cortos (Alarmas_SetTiempos). Por defecto ESCENARIO_NORMAL |
 * | 10/11/2026 | Ya no se retoma la infusión tras un reinicio: se la da por interrumpida (REINICIO + alarma EQUIPO_REINICIADO) y se pide la descarga. Después del FIN el minuto sigue contando hasta el OK de la app (módulo pendiente), con aviso "Registro pendiente de descarga" y confirmación al empezar una infusión nueva o apagar |
 * | 07/11/2026 | USAR_BLE = 0 compila el firmware sin Bluetooth; en los escenarios de prueba el registro se muestra por consola (cada línea que se guarda y las últimas 60 al arrancar) |
 * | 07/11/2026 | BLE con la app de la enfermera (componente aparte comunicacion): ID de infusión, gotas totales en el FIN y sentido del desvío en el registro; Comunicacion_Init después del autotest y Comunicacion_Apagar antes del reposo |
 * | 05/11/2026 | Los escenarios de prueba también acortan los tiempos del goteo sin monitoreo, la interfaz, el recordatorio del Ajuste, la muestra del registro y el aviso de reanudación |
 * | 05/11/2026 | El registro pasa a una partición propia de la flash (partitions.csv): una muestra por minuto y eventos de 16 bytes; la NVS guarda solo la configuración y dónde empieza la infusión. Al retomar, el minuto y el contador salen de la última muestra. El Resumen no deja iniciar si no hay lugar ("Memoria llena") |
 * | 05/11/2026 | Etapa 6: Apagar muestra "Registro guardado" 2 s y pasa a reposo (pantalla en negro, sensado y buzzer apagados, light sleep; el botón 3 s lo reinicia); Modo Técnico con el código técnico en la clave del arranque (Diagnóstico, Probar alarma, Borrar registro y Salir; 5 min sin actividad reinician); no se pide de nuevo el botón 3 s después del reposo o de Salir |
 * | 05/11/2026 | Etapa 5: registro de eventos y reanudación. La infusión activa (configuración, minuto y gotas) se guarda en la memoria al iniciar y cada 1 min (componente registro); si el equipo se reinicia la retoma sola, sin clave ni Config (volumen incierto, aviso "Monitoreo reanudado", alarma FALLA_EQUIPO si el autotest falla). Alarma de goteo sin monitoreo. Cada alarma, silenciado, clave, inicio, ajuste, cambio de datos y fin se anota en el registro |
 * | 05/11/2026 | Etapa 4: autotest real con cinco chequeos (pantalla, acelerómetro, sensor óptico con su nivel base, memoria NVS y batería), con "Falla: <nombre>" y la acción; Resumen en dos páginas con el factor de goteo destacado; recordatorio del Ajuste de goteo a los 10 min |
 * | 05/11/2026 | Etapa 3: menú de Ajustes (Cambiar datos, Detener monitoreo, Apagar, Volver) con confirmaciones y pantalla Sin infusión activa; Cambiar datos sigue la misma infusión (Alarmas_CambiarDuracion) y el monitoreo sigue mientras se edita |
 * | 05/11/2026 | La alarma de desvío muestra el % de desvío y las gotas/min del objetivo |
 * | 05/11/2026 | Etapa 2 de la especificación de inicio y navegación: el autotest se hace en la pantalla de inicio y pasa directo a la clave clínica (antes mostraba una pantalla con los OK y pedía un click); la pulsación larga de 3 s reemplaza al doble click |
 * | 05/11/2026 | Sonidos de la interfaz: tonos ascendentes al encender, descendentes al apagar, pitido con el botón y tic con el giro (BuzzerAlarm_Efecto) |
 * | 05/11/2026 | Se integra la batería (componente bateria, GPIO_4): carga real en pantalla y alarmas del 20, 10 y 5 %. Se saca la batería simulada y ESCENARIO_BATERIA. La alarma de desvío fuerte pasa a 30 %. El click en la pantalla de Colocación durante la infusión la silencia |
 * | 04/11/2026 | Con la interfaz nueva, el click solo silencia (también fin de sachet), la colocación se resuelve sola y "Finalizar infusión" del Menú técnico apaga el sonido y las alarmas, para poder seguir probando las otras alarmas sin reiniciar |
 * | 27/11/2026 | El programa principal se divide en módulos (escenarios.h, estado, autotest, infusion, gestion_alarmas, descarga, modo_tecnico y reposo): monitor_goteo.c queda solo con app_main y el lazo principal. Sin cambios de comportamiento |
 *
 * @section modulos Módulos de main/
 *
 * - escenarios.h: interruptores USAR_BLE y ESCENARIO y los tiempos que cambian con el escenario.
 * - estado.h / estado.c: lo único compartido (datos de la interfaz y midiendo).
 * - autotest.c: los cinco chequeos del arranque.
 * - infusion.c: cola de gotas, inicio y fin de la infusión, registro de eventos y muestra de cada minuto.
 * - gestion_alarmas.c: datos para el componente alarmas, alarma en la interfaz y el buzzer, silenciado, ángulo y batería.
 * - descarga.c y pendiente.c: enlace con el Bluetooth y registro pendiente de descarga.
 * - modo_tecnico.c y reposo.c: pedidos del Modo Técnico, reposo y reinicio sin pedir el botón 3 s.
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
#include "alarmas.h"
#include "buzzer_alarm.h"
#include "drop_detection.h"
#include "inclinacion.h"
#include "bateria.h"
#include "registro.h"
#include "pendiente.h"
#include "display.h"
#include "i2c_mcu.h"
#include "escenarios.h"
#include "estado.h"
#include "autotest.h"
#include "infusion.h"
#include "gestion_alarmas.h"
#include "descarga.h"
#include "modo_tecnico.h"
#include "reposo.h"

/*==================[macros and definitions]=================================*/
#define PERIODO_TICK_MS       1000  /*!< Cada cuánto se evalúan las alarmas y se actualiza la interfaz */
#define ESPERA_MAX_MS         100   /*!< Máximo que se espera un evento del encoder antes de mirar la cola de gotas */
#define REFRESCO_MIN_MS       250   /*!< Mínimo entre dos redibujados de pantalla provocados por gotas */

#define TOLERANCIA_GRADOS     8.0f  /*!< Ángulo máximo (hacia cualquier lado) para dar la colocación por correcta. En las tomas de
                                         ../analisis_bpw34 (c1, c2, c4, c5) la detección empieza a perderse a 9.8° del lado positivo y
                                         a -10.8° del negativo (pocos cortes por lado): se deja ~2° de margen. Antes 2.5° provisorio */

#if !USAR_BLE
#warning "monitor_goteo compilado SIN Bluetooth (USAR_BLE = 0): volver a 1 para la versión final"
#endif

#if ESCENARIO != ESCENARIO_NORMAL
#warning "monitor_goteo compilado en un ESCENARIO DE PRUEBA (tiempos de alarma acortados): volver a ESCENARIO_NORMAL para la versión final"
#endif

static const char *TAG = "monitor_goteo";

/*==================[internal data definition]===============================*/
#if ESCENARIO != ESCENARIO_NORMAL
/** Tiempos de alarma cortos para probar sin esperar minutos (los de la especificación son 120, 300, 120 y 120 s) */
static const alarmas_tiempos_t tiempos_prueba = {
    .desv_estabilizacion_s = 20,
    .desv_leve_persist_s = 30,
    .desv_fuerte_persist_s = 20,
    .rearme_s = 20,
    .gsm_persist_s = 20,    /* goteo sin monitoreo: 2 min en la especificación */
    .gsm_hueco_s = 15,      /* y se resuelve a los 15 s sin gotas (45 s en la especificación) */
    .bateria_persist_s = 5, /* cambio de escalón de batería: 30 s en la especificación */
};
#endif
static int64_t ultimo_refresco_us = 0;
static uint32_t fallos_oled_avisados = 0;   /*!< Fallos de envío a la OLED que ya se informaron por consola */

/*==================[internal functions definition]==========================*/
static void DatosIniciales(void) {
    datos = (ui_datos_t){
        .factor_goteo = CONFIG_FACTOR,
        .volumen_total_ml = CONFIG_VOLUMEN_ML,
        .duracion_min = CONFIG_DURACION_MIN,
        .flujo_objetivo_mlh = (CONFIG_VOLUMEN_ML * 60 + CONFIG_DURACION_MIN / 2) / CONFIG_DURACION_MIN,   // volumen / tiempo; después lo recalcula la interfaz al cambiar Config
        .umbral_sin_gotas_s = 15,
        .bateria_pct = 100,
        .angulo_grados = 0.0f,
        .colocacion_ok = true,
        .tolerancia_grados = TOLERANCIA_GRADOS,
    };
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    bool sin_espera = Reposo_ReinicioAProposito();   // reinicio a propósito: no se pide de nuevo el botón 3 s
    bool oled_ok = UI_Init();
    if (!oled_ok) {
        ESP_LOGE(TAG, "la OLED no responde");
    }
    Encoder_Init();
    BuzzerAlarm_Init(BUZZER_ALARM_GPIO);
    bool memoria_ok = Registro_Init();
    if (!memoria_ok) {
        ESP_LOGE(TAG, "la memoria no volatil (NVS) no responde: no se puede guardar el registro (se reporta en el autotest)");
    }
#if ESCENARIO != ESCENARIO_NORMAL
    // Modo de prueba: cada línea que se guarda en el registro sale por la consola y al arrancar se muestran las últimas
    // 60 guardadas (así se comprueba la memoria y que sobrevive a un reinicio sin necesitar el BLE ni la app)
    Registro_ActivarLog(true);
#endif
    Alarmas_SetCallback(Infusion_EventoAlarma);
    UI_SetCallbackClave(Infusion_EventoClave);
#if ESCENARIO != ESCENARIO_NORMAL
    Alarmas_SetTiempos(&tiempos_prueba);
    UI_SetTiempos(20, 30, 15);   // inactividad de clave/Ajustes/Sin infusión 20 s, Modo Técnico 30 s, bloqueo de clave 15 s
    UI_VolumenPrueba10ml(true);  // 10 mL como opción de volumen, para probar con la bomba
    ESP_LOGW(TAG, "ESCENARIO DE PRUEBA: %s (tiempos de alarma acortados, no es la versión final)", NOMBRE_ESCENARIO);
#endif
    Bateria_Init();
    Infusion_Init();   // la cola de gotas tiene que existir antes de iniciar el detector
    bool sensor_ok = DropDetection_Init(Infusion_AvisoGota);
    bool acel_ok = Inclinacion_Init();   // después del sensor óptico: así sus lecturas por I2C no meten ruido cuando mide el nivel base
    if (!acel_ok) {
        ESP_LOGE(TAG, "el acelerómetro no responde (se reintenta en el autotest)");
    }

    // Si había una infusión sin terminar (corte de batería, watchdog, brown-out) YA NO se retoma: se la da por interrumpida, suena la
    // alarma de reinicio y se pide la descarga. Lo mismo con un registro terminado y sin descargar. En esos casos no se espera el botón
    registro_infusion_t activa;
    bool sin_terminar = memoria_ok && Registro_CargarInfusion(&activa);
    Pendiente_Init();
    bool pendiente = memoria_ok && Pendiente_AlEncender(sin_terminar, esp_timer_get_time());
    if (!pendiente && !sin_espera) {
        // "Encendido": la pantalla queda apagada hasta mantener el botón apretado
        ESP_LOGI(TAG, "Mantener el botón %d ms para encender", ENCENDIDO_MS);
        Encoder_EsperarMantenido(ENCENDIDO_MS);
    } else if (pendiente) {
        ESP_LOGW(TAG, "registro pendiente de descarga%s: se pide la descarga con la app", sin_terminar ? " (infusion interrumpida por un reinicio)" : "");
    }
    BuzzerAlarm_Efecto(BUZZER_EFECTO_ENCENDIDO);   // tonos ascendentes mientras se muestra la pantalla de inicio

    Autotest_Init(oled_ok, sensor_ok, acel_ok);
    DatosIniciales();
    datos.registro_minutos_libres = Registro_MinutosLibres();
    datos.registro_pendiente = pendiente;
    Autotest_Ejecutar(!pendiente);  // se hace mientras se muestra "Iniciando": si todo da OK pasa directo a la clave (con un registro pendiente no bloquea)
    GestionAlarmas_ActualizarAngulo();   // para que la pantalla de Colocación no parta de un "correcto" por defecto
    Encoder_EsperarMantenido(0); // espera a que se suelte el botón (si se venía de mantenerlo) y descarta lo que se acumuló
    Descarga_IniciarBle();       // después del autotest, para que la radio no moleste la medición del nivel base
    // Estado del registro: se muestra acá (y no al arrancar) porque la consola USB pierde las primeras líneas del arranque
    ESP_LOGI(TAG, "registro: %lu lineas escritas, %lu minutos de muestras libres", (unsigned long)Registro_Total(),
             (unsigned long)Registro_MinutosLibres());
#if ESCENARIO != ESCENARIO_NORMAL
    Registro_Volcar(60);
#endif
    if (pendiente) {
        UI_IniciarSinInfusion(&datos);   // "Sin infusión activa" con la alarma de reinicio (si corresponde) y el aviso del registro pendiente
    } else {
        UI_IniciarClave(&datos);
    }

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
            // Sonido de la interfaz: pitido con el botón y tic con el giro (no tapan una alarma que esté sonando)
            BuzzerAlarm_Efecto((ev == ENC_GIRO_HORARIO || ev == ENC_GIRO_ANTIHORARIO) ? BUZZER_EFECTO_GIRO : BUZZER_EFECTO_CLICK);
            UI_Evento(ev, &datos);
            Infusion_RevisarTransiciones(pantalla);
            ModoTecnico_Atender();
            Descarga_Atender(esp_timer_get_time());

            if (!silenciada_antes && datos.alarma_silenciada) {
                GestionAlarmas_Silenciar(esp_timer_get_time());
            }
        }

        Bateria_Muestrear(BuzzerAlarm_Sonando());   // no se mide mientras suena el buzzer: sus picos de consumo bajan la tensión

        int64_t ahora = esp_timer_get_time();
        int nuevas = Infusion_ProcesarGotas();
        if (nuevas > 0 && (ahora - ultimo_refresco_us) / 1000 >= REFRESCO_MIN_MS) {
            // Una gota nueva: se muestra enseguida en el Monitoreo (volumen, caudal) sin esperar al próximo tick
            ui_pantalla_t p = UI_PantallaActual();
            if (p == UI_PANTALLA_MONITOREO || p == UI_PANTALLA_AJUSTE_GOTEO) {
                Infusion_ActualizarMedicion(ahora);
                UI_Actualizar(&datos);
                ultimo_refresco_us = esp_timer_get_time();
            }
        }

        if ((ahora - ultimo_tick_us) / 1000 >= PERIODO_TICK_MS) {
            ultimo_tick_us = ahora;
            GestionAlarmas_ActualizarAngulo();
            GestionAlarmas_ActualizarBateria();
            Infusion_RecordarAjuste(ahora);
            ModoTecnico_ActualizarDiagnostico();
            Infusion_ActualizarMedicion(ahora);
            datos.registro_minutos_libres = Registro_MinutosLibres();
            Infusion_GuardarMuestraSiToca(ahora);
            Descarga_Atender(ahora);
            GestionAlarmas_Evaluar(ahora, false);
            UI_Actualizar(&datos);
            ModoTecnico_Atender();
            ultimo_refresco_us = esp_timer_get_time();

            // Diagnóstico: si se perdieron envíos a la OLED desde el último aviso, se dice cuántos
            if (Display_FallosI2C() != fallos_oled_avisados) {
                fallos_oled_avisados = Display_FallosI2C();
                // Se dice también por qué fallan: guarda = la guarda vio SCL en bajo y no inició la operación;
                // driver = la operación llegó al driver de ESP-IDF y falló (sin respuesta o tiempo agotado)
                ESP_LOGW(TAG, "fallaron envios a la OLED por I2C: %lu en total (bus: guarda SCL=%lu, driver=%lu, ultimo error del driver=%d)",
                         (unsigned long)fallos_oled_avisados, (unsigned long)I2C_FallosGuardaSCL(),
                         (unsigned long)I2C_FallosDriver(), I2C_UltimoErrorDriver());
            }

            // Diagnóstico: cada 30 s, lo mínimo que le quedó libre de pila a esta tarea (si se acerca a 0, hay riesgo de reinicio)
            static uint32_t ticks_pila = 0;
            if (++ticks_pila % 30 == 0) {
                ESP_LOGI(TAG, "pila libre minima de la tarea principal: %u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
            }
        }

        if (UI_ApagarPedido()) {
            // Apagar: si había una infusión activa ya se finalizó (se anotó FIN y se borró la infusión activa). El registro se
            // va guardando en la memoria con cada evento: se confirma con el mensaje y el equipo pasa a reposo
            BuzzerAlarm_Stop();
            BuzzerAlarm_Efecto(BUZZER_EFECTO_APAGADO);   // tonos descendentes; se espera que terminen antes de apagar
            UI_PantallaMensaje("Registro guardado");
            BuzzerAlarm_EsperarFin(1000);
            vTaskDelay(pdMS_TO_TICKS(2000));
            Reposo_Entrar();
        }
    }
}
/*==================[end of file]============================================*/
