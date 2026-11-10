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
 * ESCENARIO (macro de más abajo) elige con qué arranca la configuración y si los tiempos de las
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
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "encoder.h"
#include "ui.h"
#include "goteo.h"
#include "alarmas.h"
#include "buzzer_alarm.h"
#include "drop_detection.h"
#include "optical_sensor.h"
#include "inclinacion.h"
#include "bateria.h"
#include "registro.h"
#include "pendiente.h"
#include "comunicacion.h"
#include "display.h"
#include "i2c_mcu.h"

/*==================[macros and definitions]=================================*/
#define ENCENDIDO_MS          3000  /*!< Tiempo que hay que mantener apretado el botón para encender */
#define DURACION_INICIO_MS    5000  /*!< Duración mínima de la pantalla de inicio (autotest) */
#define PERIODO_CHEQUEO_MS    1000  /*!< Cada cuánto se hace o reintenta un chequeo del autotest */
#define PERIODO_ANIMACION_MS  42    /*!< Cada cuánto avanza un cuadro de la animación del inicio */
#define PERIODO_TICK_MS       1000  /*!< Cada cuánto se evalúan las alarmas y se actualiza la interfaz */
#define ESPERA_MAX_MS         100   /*!< Máximo que se espera un evento del encoder antes de mirar la cola de gotas */
#define REFRESCO_MIN_MS       250   /*!< Mínimo entre dos redibujados de pantalla provocados por gotas */
#define COLA_GOTAS_LARGO      32
#define BASE_OPTICA_MIN_MV    50      /*!< Nivel base del fotodiodo por debajo de esto: en cero (sensor sin señal). Provisorio: se fija con ensayos de banco */
#define BASE_OPTICA_MAX_MV    2400    /*!< Por encima de esto: saturado (el ADC con 12 dB llega a ~2500 mV). Provisorio */
#define BATERIA_MIN_ARRANQUE  10      /*!< El autotest pide más que este % de batería (o cargador, que sube la tensión) */
#define AJUSTE_RECORDATORIO_S 600     /*!< Segundos en el Ajuste de goteo sin confirmar para empezar a recordarlo */
#define AJUSTE_BEEP_CADA_S    60      /*!< Cada cuánto suena el beep del recordatorio */
#define NOMBRE_BLE            "MonitorGoteo-01"   /*!< Nombre del equipo para la app: cambiarlo en cada equipo (hasta 20 caracteres) */
#define FIRMWARE_VERSION      "0.1"   /*!< Versión que muestra el Diagnóstico del Modo Técnico */
#define MARCA_SIN_ESPERA      0x5AFE0B07   /*!< Marca (en memoria que sobrevive al reinicio) para no pedir de nuevo el botón 3 s después de salir del reposo o del Modo Técnico */
#define GUARDAR_CADA_S        60      /*!< Cada cuánto se escribe una muestra en el registro (contador, caudal y volumen): de ahí sale el último minuto guardado si el equipo se reinicia */

#define TOLERANCIA_GRADOS     8.0f  /*!< Ángulo máximo (hacia cualquier lado) para dar la colocación por correcta. En las tomas de
                                         ../analisis_bpw34 (c1, c2, c4, c5) la detección empieza a perderse a 9.8° del lado positivo y
                                         a -10.8° del negativo (pocos cortes por lado): se deja ~2° de margen. Antes 2.5° provisorio */
/* Escenario: qué configuración arranca precargada y si los tiempos de alarma se acortan (ver arriba).
 * Cambiar esta línea para probar y VOLVER A ESCENARIO_NORMAL para la versión de verdad. */
/* Bluetooth con la app: 1 = el firmware final. 0 = firmware SIN Bluetooth (como antes del BLE), para probar el resto
 * del equipo. VOLVER A 1 para la versión final. */
#define USAR_BLE                     1

#define ESCENARIO_NORMAL             0
#define ESCENARIO_OCLUSION_FIN       1
#define ESCENARIO_DESVIO_COLOCACION  2
#define ESCENARIO                    ESCENARIO_DESVIO_COLOCACION   /*!< por defecto, la versión final es ESCENARIO_NORMAL */

#if ESCENARIO == ESCENARIO_OCLUSION_FIN
#define NOMBRE_ESCENARIO      "OCLUSION_FIN"
#define CONFIG_FACTOR         10    /*!< gtt/mL: con 50 mL son solo 500 gotas */
#define CONFIG_VOLUMEN_ML     50
#define CONFIG_DURACION_MIN   10    /*!< objetivo 50 gtt/min */
#elif ESCENARIO == ESCENARIO_DESVIO_COLOCACION
#define NOMBRE_ESCENARIO      "DESVIO_COLOCACION"
#define CONFIG_FACTOR         20
#define CONFIG_VOLUMEN_ML     500
#define CONFIG_DURACION_MIN   (8 * 60)   /*!< objetivo 20,8 gtt/min: con un goteo de ~56 gtt/min hay desvío alto */
#else
#define CONFIG_FACTOR         20
#define CONFIG_VOLUMEN_ML     500
#define CONFIG_DURACION_MIN   (8 * 60)
#endif

#if !USAR_BLE
#warning "monitor_goteo compilado SIN Bluetooth (USAR_BLE = 0): volver a 1 para la versión final"
#endif

#if ESCENARIO != ESCENARIO_NORMAL
#warning "monitor_goteo compilado en un ESCENARIO DE PRUEBA (tiempos de alarma acortados): volver a ESCENARIO_NORMAL para la versión final"
#endif

/* Escenarios de prueba: tiempos cortos para no esperar minutos (los de la especificación son los de arriba) */
#if ESCENARIO != ESCENARIO_NORMAL
#undef AJUSTE_RECORDATORIO_S
#undef AJUSTE_BEEP_CADA_S
#undef GUARDAR_CADA_S
#define AJUSTE_RECORDATORIO_S 30
#define AJUSTE_BEEP_CADA_S    10
#define GUARDAR_CADA_S        15
#endif

static const char *TAG = "monitor_goteo";

/*==================[internal data definition]===============================*/
static ui_datos_t datos;
#if ESCENARIO != ESCENARIO_NORMAL
/** Tiempos de alarma cortos para probar sin esperar minutos (los de la especificación son 120, 300, 120 y 120 s) */
static const alarmas_tiempos_t tiempos_prueba = {
    .desv_estabilizacion_s = 20,
    .desv_leve_persist_s = 30,
    .desv_fuerte_persist_s = 20,
    .rearme_s = 20,
    .gsm_persist_s = 20,    /* goteo sin monitoreo: 2 min en la especificación */
    .gsm_hueco_s = 15,      /* y se resuelve a los 15 s sin gotas (45 s en la especificación) */
};
#endif
/* Chequeos del autotest, en el orden de la especificación. Si uno falla se muestra "Falla: <nombre>" y la acción */
static ui_chequeo_t chequeos[UI_CHEQUEOS_MAX] = {
    {"Pantalla", UI_CHEQUEO_PENDIENTE, NULL},
    {"Acelerometro", UI_CHEQUEO_PENDIENTE, NULL},
    {"Sensor optico", UI_CHEQUEO_PENDIENTE, NULL},
    {"Memoria", UI_CHEQUEO_PENDIENTE, NULL},
    {"Bateria baja", UI_CHEQUEO_PENDIENTE, "Conectar cargador"},
};
static bool oled_ok = false;
static bool encoder_ok = false;
static bool sensor_ok = false;
static bool acel_ok = false;
static bool angulo_valido = false;     /*!< true si la última lectura del acelerómetro es confiable */
RTC_NOINIT_ATTR static uint32_t marca_sin_espera;   /*!< MARCA_SIN_ESPERA si el reinicio fue a propósito (reposo o Salir del Modo Técnico) */
static uint8_t prueba_sonando = 0;     /*!< Modo Técnico: patrón que está sonando (0 = ninguno, n = patrón n-1) */
static bool memoria_ok = false;        /*!< true si la memoria no volátil (NVS) quedó lista */
static volatile bool ok_recibido = false;   /*!< La app confirmó la descarga (OK por BLE): lo atiende el lazo principal */
static bool pitando_falla = false;     /*!< true mientras el buzzer avisa que la pantalla falló en el autotest */
static int64_t ultimo_guardado_us = 0; /*!< Última vez que se escribió la muestra de 1 min en el registro */
static uint32_t inicio_linea_activa = 0; /*!< Línea del registro donde empezó la infusión activa */
static uint32_t id_activa = 0;         /*!< ID de la infusión activa (o de la última) */
static uint8_t direccion_desvio[ALARMAS_CANTIDAD];   /*!< 0 = alto, 1 = bajo: sentido del desvío cuando se activó (para el registro) */
static uint16_t duracion_guardada = 0; /*!< Duración en curso (para anotar CAMBIO_DATOS con el valor anterior) */
static int64_t ajuste_ini_us = 0;      /*!< Hora en que se entró al Ajuste de goteo (para el recordatorio de los 10 min) */
static int64_t ultimo_recordatorio_us = 0;
static bool midiendo = false;          /*!< true desde que empieza el Ajuste de goteo hasta que termina la infusión */
static bool tiempo_iniciado = false;   /*!< true desde que arrancó el tiempo de la infusión (la primera vez que se entra al Monitoreo) */
static QueueHandle_t cola_gotas = NULL;   /*!< Hora de cada gota detectada (de la tarea del sensor al main) */
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

/* Chequeos del autotest. Cada uno devuelve true si el componente responde. */
static bool ChequearOled(void) {
    return oled_ok;
}

static bool ChequearAcelerometro(void) {
    if (!acel_ok) {
        acel_ok = Inclinacion_Init();   // el autotest reintenta cada segundo: por si el chip tardó en responder
    }
    return acel_ok && Inclinacion_Valida(NULL);
}

/** Carga en datos el ángulo del equipo y si la colocación es correcta. Si el acelerómetro no
 * responde, la colocación no se puede verificar: se da por incorrecta en vez de ignorarlo. */
static void ActualizarAngulo(void) {
    float grados = 0.0f;
    angulo_valido = Inclinacion_Valida(&grados);
    datos.angulo_grados = angulo_valido ? grados : 0.0f;
    datos.colocacion_ok = angulo_valido && fabsf(grados) <= datos.tolerancia_grados;
}

/** El nivel base de los dos fotodiodos (luz recibida sin gota) no está en cero ni saturado. Vale con o sin cámara colocada */
static bool ChequearSensorOptico(void) {
    if (!sensor_ok) {
        return false;
    }
    for (int ch = 0; ch < OPTICAL_SENSOR_CHANNELS; ch++) {
        float base = OpticalSensor_BaseMv(ch);
        ESP_LOGI(TAG, "sensor optico: nivel base del canal %d = %.0f mV (rango %d a %d mV)", ch + 1, (double)base,
                 BASE_OPTICA_MIN_MV, BASE_OPTICA_MAX_MV);
        if (base < BASE_OPTICA_MIN_MV || base > BASE_OPTICA_MAX_MV) {
            return false;
        }
    }
    return true;
}

/** La memoria no volátil (NVS) se puede iniciar, escribir y leer */
static bool ChequearMemoria(void) {
    return Registro_Probar();
}

/** Batería con más de BATERIA_MIN_ARRANQUE % (o cargador conectado: sube la tensión). Sin batería conectada (solo USB) pasa */
static bool ChequearBateria(void) {
    return !Bateria_Valida() || Bateria_Pct() > BATERIA_MIN_ARRANQUE;
}

static bool (*const funciones_chequeo[UI_CHEQUEOS_MAX])(void) = {
    ChequearOled, ChequearAcelerometro, ChequearSensorOptico, ChequearMemoria, ChequearBateria,
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
static void AutotestInicio(bool bloquear) {
    uint32_t proximo_ms[UI_CHEQUEOS_MAX];
    uint32_t t_ms;
    int64_t inicio_us = esp_timer_get_time();

    for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
        proximo_ms[i] = i * PERIODO_CHEQUEO_MS + 500;   // se escalonan; el de batería espera a tener lecturas
    }

    do {
        Bateria_Muestrear(false);   // la carga se mide mientras se hace el autotest
        t_ms = (esp_timer_get_time() - inicio_us) / 1000;
        for (uint8_t i = 0; i < UI_CHEQUEOS_MAX; i++) {
            if (chequeos[i].estado != UI_CHEQUEO_OK && t_ms >= proximo_ms[i]) {
                chequeos[i].estado = funciones_chequeo[i]() ? UI_CHEQUEO_OK : UI_CHEQUEO_FALLA;
                proximo_ms[i] = t_ms + PERIODO_CHEQUEO_MS;
            }
        }
        // Si la pantalla falla no hay cómo mostrar un texto: avisa con el buzzer hasta que responda
        if (chequeos[0].estado == UI_CHEQUEO_FALLA && !pitando_falla) {
            pitando_falla = true;
            BuzzerAlarm_Start(BUZZER_ALARM_FLUJO_DETENIDO);
        } else if (chequeos[0].estado == UI_CHEQUEO_OK && pitando_falla) {
            pitando_falla = false;
            BuzzerAlarm_Stop();
        }
        if (bloquear && t_ms >= DURACION_INICIO_MS && !TodosOk()) {
            UI_Ir(UI_PANTALLA_AUTOTEST, &datos);   // un chequeo falla: se muestra cuál (se reintenta cada segundo)
        } else {
            UI_PantallaInicio(t_ms / PERIODO_ANIMACION_MS);
        }
        // Normal: no deja continuar con una falla. Con un registro pendiente (reinicio) no hay enfermera para leerla: sigue y suena la alarma
    } while (t_ms < DURACION_INICIO_MS || (bloquear && !TodosOk()));
    if (pitando_falla) {
        pitando_falla = false;
        BuzzerAlarm_Stop();
    }
}

/** Minutos desde el inicio de la infusión (0 si todavía no empezó el tiempo) */
static uint16_t MinutoInfusion(int64_t ahora_us) {
    if (!tiempo_iniciado) {
        return 0;
    }
    uint32_t m = Goteo_TiempoS(ahora_us) / 60;
    return (m > UINT16_MAX) ? UINT16_MAX : (uint16_t)m;
}

#if USAR_BLE
/** Lo que le informamos a la comunicación BLE (componente aparte): estado de la última infusión, el minuto actual y el OK */
static uint8_t EstadoParaBle(void) {
    if (midiendo && datos.infusion_configurada) {
        return REGISTRO_ESTADO_EN_CURSO;       // 1 = en curso
    }
    return Pendiente_Estado();                 // 0 = terminada, 2 = interrumpida por un reinicio
}

/** Minuto actual: en curso = desde el inicio; terminada = desde el inicio (sigue contando hasta el OK); interrumpida = desde el reinicio */
static uint32_t MinutoParaBle(void) {
    if (midiendo && datos.infusion_configurada) {
        return MinutoInfusion(esp_timer_get_time());
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
    Registro_Muestra(MinutoInfusion(ahora_us), gotas, (caudal_x10 > UINT16_MAX) ? UINT16_MAX : (uint16_t)caudal_x10,
                     (volumen_x10 > UINT16_MAX) ? UINT16_MAX : (uint16_t)volumen_x10);
    ultimo_guardado_us = ahora_us;
}

/** Lo llama alarmas con cada aviso o alarma que se activa o se resuelve: queda en el registro */
static void EventoAlarma(alarma_evento_t evento, bool activo, int64_t ahora_us) {
    if (activo && (evento == ALARMA_DESVIACION_FUERTE || evento == AVISO_DESVIACION_LEVE)) {
        direccion_desvio[evento] = (Alarmas_Desvio() < 0.0f) ? 1 : 0;   // 0 = flujo alto, 1 = flujo bajo (lo pide la app)
    }
    Registro_Anotar(REG_ALARMA, activo, MinutoInfusion(ahora_us), (uint16_t)evento, direccion_desvio[evento]);
    ESP_LOGI(TAG, "%s %s", Alarmas_Nombre(evento), activo ? "ACTIVADA" : "resuelta");
}

/** Lo llama la interfaz con cada clave ingresada: queda en el registro */
static void EventoClave(ui_clave_resultado_t resultado) {
    registro_evento_t e = (resultado == UI_CLAVE_OK) ? REG_CLAVE_OK : (resultado == UI_CLAVE_ERROR) ? REG_CLAVE_ERROR : REG_TECNICO_OK;
    Registro_Anotar(e, 0, MinutoInfusion(esp_timer_get_time()), 0, 0);
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

/** Mira si cambió la pantalla por un evento y arranca lo que corresponda. */
static void RevisarTransiciones(ui_pantalla_t antes) {
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
            Registro_Anotar(REG_CAMBIO_DATOS, 0, MinutoInfusion(esp_timer_get_time()), duracion_guardada, datos.duracion_min);
            duracion_guardada = datos.duracion_min;
            GuardarInfusion();
        }
    } else if (antes == UI_PANTALLA_AJUSTE_GOTEO && ahora == UI_PANTALLA_MONITOREO) {
        if (!tiempo_iniciado) {
            Goteo_ReiniciarTiempo(esp_timer_get_time());   // el tiempo de la infusión arranca recién acá
            tiempo_iniciado = true;
        }
        Registro_Anotar(REG_AJUSTE_OK, 0, MinutoInfusion(esp_timer_get_time()), 0, 0);
    }
    if (!datos.infusion_configurada && midiendo) {
        // Se detuvo el monitoreo (o se apagó): se anota el fin, se borra la infusión activa, se dejan de medir gotas
        // y se apaga lo que estuviera sonando
        const int64_t ahora_fin = esp_timer_get_time();
        const uint32_t minuto_fin = MinutoInfusion(ahora_fin);
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

/** true si no hay infusión activa y se vigila que no caigan gotas (pantalla Sin infusión y clave del arranque).
 * No en el autotest, la Colocación, Config ni Resumen: ahí la enfermera está presente */
static bool SinInfusionEvaluable(void) {
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
        } else if (SinInfusionEvaluable()) {
            Alarmas_GotaSinMonitoreo(t_us);   // gotas sin infusión activa: si son continuas 2 min, alarma
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
    case ALARMA_EQUIPO_REINICIADO:  return UI_ALARMA_EQUIPO_REINICIADO;
    case ALARMA_GOTEO_SIN_MONITOREO: return UI_ALARMA_GOTEO_SIN_MONITOREO;
    default:                        return UI_ALARMA_NINGUNA;
    }
}

/** Pasa a la interfaz la alarma que corresponde mostrar: la de mayor prioridad sin silenciar o,
 * si no hay, la silenciada de mayor prioridad (queda el aviso abajo en el Monitoreo). */
static void CargarAlarmaEnInterfaz(void) {
    datos.aviso_bateria = (Alarmas_Activas() & ALARMA_BIT(AVISO_BATERIA)) != 0;
    datos.desvio_pct = (int16_t)lroundf(Alarmas_Desvio() * 100.0f);   // la alarma de desvío lo muestra
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

/** Si pasaron 10 min en el Ajuste de goteo sin confirmar, el pie lo recuerda y suena un beep breve por minuto,
 * para que el equipo no quede sin monitorear si la enfermera se distrae */
static void RecordarAjuste(int64_t ahora) {
    bool recordar = UI_PantallaActual() == UI_PANTALLA_AJUSTE_GOTEO && (ahora - ajuste_ini_us) > (int64_t)AJUSTE_RECORDATORIO_S * 1000000;
    datos.recordatorio_ajuste = recordar;
    if (recordar && (ultimo_recordatorio_us == 0 || (ahora - ultimo_recordatorio_us) >= (int64_t)AJUSTE_BEEP_CADA_S * 1000000)) {
        ultimo_recordatorio_us = ahora;
        BuzzerAlarm_Efecto(BUZZER_EFECTO_CLICK);
    }
}

/** Carga en datos la carga de la batería. Sin batería conectada (tensión menor a la de una celda de
 * Li-ion) queda en 100 %, para que el equipo conectado solo al USB no dé alarmas de batería. */
static void ActualizarBateria(void) {
    datos.bateria_pct = Bateria_Valida() ? Bateria_Pct() : 100;
}

/** true si la infusión está en curso y hay que evaluar las alarmas (desde el Monitoreo hasta el fin) */
static bool EnMonitoreo(void) {
    if (!midiendo || !datos.infusion_configurada) {
        return false;
    }
    if (UI_EditandoDatos()) {
        return true;   // cambiando la duración desde Ajustes: el monitoreo sigue
    }
    switch (UI_PantallaActual()) {
    case UI_PANTALLA_MONITOREO:
    case UI_PANTALLA_ALARMA:
    case UI_PANTALLA_ACCESO:
    case UI_PANTALLA_AJUSTES:
    case UI_PANTALLA_DETENER:
    case UI_PANTALLA_APAGAR:
    case UI_PANTALLA_COLOCACION:
        return true;
    default:
        return false;
    }
}

/** Una vez por segundo: evalúa las alarmas, las pasa a la interfaz y las hace sonar. */
static void EvaluarAlarmas(int64_t ahora, bool forzar) {
    // forzar: se evalúa aunque la pantalla no sea de "sin infusión" (para apagar una alarma cuando llega el OK o se descarta el registro)
    bool sin_infusion = SinInfusionEvaluable() || (forzar && !midiendo);
    if (!midiendo && !sin_infusion) {
        // Pantallas donde no se vigila (Colocación, Config, Resumen, autotest): si quedó una alarma activa o sonando se apaga,
        // para que no siga sonando ni tape los sonidos de la interfaz (por ejemplo el beep del recordatorio del Ajuste)
        if (Alarmas_Activas() != 0) {
            Alarmas_Iniciar(datos.factor_goteo, datos.volumen_total_ml, datos.duracion_min, ahora);
            CargarAlarmaEnInterfaz();
            AlarmasSonido_Actualizar();
        }
        return;
    }
    alarmas_entrada_t entrada = {
        .en_monitoreo = EnMonitoreo(),
        .en_pausa = datos.en_pausa,
        .angulo_grados = angulo_valido ? datos.angulo_grados : 2.0f * datos.tolerancia_grados,   // sin lectura confiable, fuera de la tolerancia
        .tolerancia_grados = datos.tolerancia_grados,
        .base_optica_ok = true,          // TODO: nivel de base del fotodiodo en rango
        .bateria_pct = datos.bateria_pct,
        .cargador = false,               // sin detección de cargador (decidido): el aumento de tensión al cargar resuelve la alarma
        .equipo_reiniciado = Pendiente_EquipoReiniciado(),
        .sin_infusion = sin_infusion,
    };
    Alarmas_Actualizar(&entrada, ahora);
    CargarAlarmaEnInterfaz();
    AlarmasSonido_Actualizar();
}

/** Registro pendiente de descarga: sigue contando minutos, atiende el OK de la app y el "Continuar igual" de la enfermera */
static void AtenderPendiente(int64_t ahora) {
    Pendiente_Tick(ahora);
    if (ok_recibido) {
        ok_recibido = false;
        Pendiente_Descargado(ahora);   // la app confirmó la descarga: se deja de contar y se apaga el aviso y la alarma de reinicio
        if (!midiendo) {
            EvaluarAlarmas(ahora, true);
        }
    }
    if (datos.descartar_pendiente) {
        datos.descartar_pendiente = false;
        Pendiente_Descartar();         // "Continuar igual": el registro sin descargar queda de lado
        if (!midiendo) {
            EvaluarAlarmas(ahora, true);
        }
    }
    datos.registro_pendiente = Pendiente_Hay();
}

/** Reinicia el equipo sin pedir de nuevo el botón 3 s (el usuario ya lo mantuvo o eligió Salir) */
static void ReiniciarSinEspera(void) {
    marca_sin_espera = MARCA_SIN_ESPERA;
    esp_restart();
}

/** Modo Técnico: hace sonar la alarma que se eligió probar, borra el registro si se confirmó y sale si corresponde */
static void AtenderModoTecnico(void) {
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
        ReiniciarSinEspera();
    }
}

/** Datos del Diagnóstico del Modo Técnico (solo lectura) */
static void ActualizarDiagnostico(void) {
    datos.base_optica_mv[0] = (uint16_t)OpticalSensor_BaseMv(0);
    datos.base_optica_mv[1] = (uint16_t)OpticalSensor_BaseMv(1);
    datos.bateria_mv = Bateria_Mv();
    datos.registro_lineas = (uint16_t)Registro_Total();
    datos.registro_descargado = Registro_Descargado();
    datos.version = FIRMWARE_VERSION;
}

/** Reposo (después de Apagar): pantalla en negro, sensado y buzzer apagados, y el micro en light sleep. Despierta
 * con el botón: si se mantiene 3 s reinicia el equipo (logo, autotest y clave); con menos vuelve a dormir sin
 * encender la pantalla. No es consumo cero: el LED infrarrojo sigue encendido por hardware y el divisor de la
 * batería consume ~210 uA; para guardar el equipo hay que abrir el interruptor interno. */
static void Reposo(void) {
    Pendiente_Tick(esp_timer_get_time());   // con un registro pendiente, deja guardado el último minuto antes de dormir
#if USAR_BLE
    Comunicacion_Apagar();   // el BLE se apaga antes del light sleep
#endif
    BuzzerAlarm_Stop();
    OpticalSensor_Stop();
    UI_PantallaApagada();
    ESP_LOGI(TAG, "reposo: mantener el boton %d ms para encender", ENCENDIDO_MS);
    Encoder_DormirHastaMantenido(ENCENDIDO_MS);
    ReiniciarSinEspera();
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    bool sin_espera = (marca_sin_espera == MARCA_SIN_ESPERA);   // reinicio a propósito: no se pide de nuevo el botón 3 s
    marca_sin_espera = 0;
    oled_ok = UI_Init();
    if (!oled_ok) {
        ESP_LOGE(TAG, "la OLED no responde");
    }
    encoder_ok = Encoder_Init();
    BuzzerAlarm_Init(BUZZER_ALARM_GPIO);
    memoria_ok = Registro_Init();
    if (!memoria_ok) {
        ESP_LOGE(TAG, "la memoria no volatil (NVS) no responde: no se puede guardar el registro (se reporta en el autotest)");
    }
#if ESCENARIO != ESCENARIO_NORMAL
    // Modo de prueba: cada línea que se guarda en el registro sale por la consola y al arrancar se muestran las últimas
    // 60 guardadas (así se comprueba la memoria y que sobrevive a un reinicio sin necesitar el BLE ni la app)
    Registro_ActivarLog(true);
#endif
    Alarmas_SetCallback(EventoAlarma);
    UI_SetCallbackClave(EventoClave);
#if ESCENARIO != ESCENARIO_NORMAL
    Alarmas_SetTiempos(&tiempos_prueba);
    UI_SetTiempos(20, 30, 15);   // inactividad de clave/Ajustes/Sin infusión 20 s, Modo Técnico 30 s, bloqueo de clave 15 s
    ESP_LOGW(TAG, "ESCENARIO DE PRUEBA: %s (tiempos de alarma acortados, no es la versión final)", NOMBRE_ESCENARIO);
#endif
    Bateria_Init();
    cola_gotas = xQueueCreate(COLA_GOTAS_LARGO, sizeof(int64_t));
    sensor_ok = DropDetection_Init(AvisoGota);
    acel_ok = Inclinacion_Init();   // después del sensor óptico: así sus lecturas por I2C no meten ruido cuando mide el nivel base
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

    UI_SetChequeos(chequeos, UI_CHEQUEOS_MAX);
    DatosIniciales();
    datos.registro_minutos_libres = Registro_MinutosLibres();
    datos.registro_pendiente = pendiente;
    AutotestInicio(!pendiente);  // se hace mientras se muestra "Iniciando": si todo da OK pasa directo a la clave (con un registro pendiente no bloquea)
    ActualizarAngulo();          // para que la pantalla de Colocación no parta de un "correcto" por defecto
    Encoder_EsperarMantenido(0); // espera a que se suelte el botón (si se venía de mantenerlo) y descarta lo que se acumuló
#if USAR_BLE
    Comunicacion_Init(NOMBRE_BLE, &enlace_ble);   // BLE con la app: después del autotest, para que la radio no moleste la medición del nivel base
#else
    ESP_LOGW(TAG, "SIN Bluetooth (USAR_BLE = 0)");
#endif
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
            RevisarTransiciones(pantalla);
            AtenderModoTecnico();
            AtenderPendiente(esp_timer_get_time());

            if (!silenciada_antes && datos.alarma_silenciada) {
                int silenciada = Alarmas_Mostrar();
                if (silenciada >= 0) {
                    Registro_Anotar(REG_SILENCIADO, 0, MinutoInfusion(esp_timer_get_time()), (uint16_t)silenciada, direccion_desvio[silenciada]);
                }
                Alarmas_Silenciar(esp_timer_get_time());
                CargarAlarmaEnInterfaz();
                AlarmasSonido_Actualizar();
                UI_Actualizar(&datos);
            }
        }

        Bateria_Muestrear(BuzzerAlarm_Sonando());   // no se mide mientras suena el buzzer: sus picos de consumo bajan la tensión

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
            ActualizarAngulo();
            ActualizarBateria();
            RecordarAjuste(ahora);
            ActualizarDiagnostico();
            ActualizarMedicion(ahora);
            datos.registro_minutos_libres = Registro_MinutosLibres();
            if (midiendo && datos.infusion_configurada && tiempo_iniciado && (ahora - ultimo_guardado_us) >= (int64_t)GUARDAR_CADA_S * 1000000) {
                GuardarMuestra(ahora);    // cada 1 min: así un reinicio pierde como mucho un minuto de la cuenta
            }
            AtenderPendiente(ahora);
            EvaluarAlarmas(ahora, false);
            UI_Actualizar(&datos);
            AtenderModoTecnico();
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
            Reposo();
        }
    }
}
/*==================[end of file]============================================*/
