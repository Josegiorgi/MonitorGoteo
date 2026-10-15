/*==================[inclusions]=============================================*/
#include "encoder.h"
#include "gpio_mcu.h"
#include "freertos/queue.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define PIN_CLK        GPIO_9
#define PIN_DT         GPIO_8
#define PIN_SW         GPIO_20

#define PERIODO_US     1000   /*!< Período de muestreo: 1 ms */
#define PASOS_POR_CLIC 4      /*!< Transiciones de cuadratura por cada clic del eje */
#define REBOTE_SW_MS   20     /*!< Tiempo estable para aceptar un cambio del botón */
#define COLA_LARGO     16

/*==================[internal data definition]===============================*/
static QueueHandle_t cola = NULL;

/* Tabla de cuadratura: índice = (estado_anterior << 2) | estado_actual, con estado = (CLK << 1) | DT.
 * +1 / -1 en las transiciones válidas del sentido horario / antihorario, 0 en las inválidas (rebotes). */
static const int8_t tabla[16] = {0, -1, 1, 0,
                                 1, 0, 0, -1,
                                -1, 0, 0, 1,
                                 0, 1, -1, 0};

static uint8_t estado_ant = 3;
static int8_t acumulado = 0;

static bool sw_estable = false;     /* true: apretado (el pin es activo en bajo) */
static uint16_t sw_cuenta = 0;      /* ms que lleva distinto de sw_estable */
static uint16_t sw_apretado_ms = 0; /* ms que lleva apretado */
static bool sw_largo_enviado = false;
static volatile bool doble_habilitado = false;
static uint16_t click_pendiente_ms = 0;   /* ms que quedan para ver si el click pendiente se vuelve doble; 0 = no hay */

/*==================[internal functions definition]==========================*/
static void Enviar(encoder_evento_t ev) {
    xQueueSend(cola, &ev, 0);   // si la cola está llena se descarta el evento
}

static void LeerGiro(void) {
    uint8_t actual = (GPIORead(PIN_CLK) << 1) | GPIORead(PIN_DT);
    if (actual == estado_ant) {
        return;
    }
    acumulado += tabla[(estado_ant << 2) | actual];
    estado_ant = actual;

    if (acumulado >= PASOS_POR_CLIC) {
        Enviar(ENCODER_INVERTIR_SENTIDO ? ENC_GIRO_ANTIHORARIO : ENC_GIRO_HORARIO);
        acumulado = 0;
    } else if (acumulado <= -PASOS_POR_CLIC) {
        Enviar(ENCODER_INVERTIR_SENTIDO ? ENC_GIRO_HORARIO : ENC_GIRO_ANTIHORARIO);
        acumulado = 0;
    } else if (actual == 3) {
        acumulado = 0;  // volvió al reposo sin completar un clic: se descarta
    }
}

static void LeerBoton(void) {
    bool apretado = !GPIORead(PIN_SW);

    if (apretado != sw_estable) {
        if (++sw_cuenta >= REBOTE_SW_MS) {
            sw_estable = apretado;
            sw_cuenta = 0;
            if (apretado) {
                sw_apretado_ms = 0;
                sw_largo_enviado = false;
            } else if (!sw_largo_enviado) {
                if (!doble_habilitado) {
                    Enviar(ENC_CLICK);
                } else if (click_pendiente_ms > 0) {
                    click_pendiente_ms = 0;   // segundo click dentro de la ventana
                    Enviar(ENC_DOBLE_CLICK);
                } else {
                    click_pendiente_ms = ENCODER_DOBLE_CLICK_MS;   // espera por si viene el segundo
                }
            }
        }
    } else {
        sw_cuenta = 0;
    }

    if (click_pendiente_ms > 0 && --click_pendiente_ms == 0) {
        Enviar(ENC_CLICK);   // no vino el segundo click: era un click simple
    }

    if (sw_estable && !sw_largo_enviado) {
        if (++sw_apretado_ms >= ENCODER_CLICK_LARGO_MS) {
            sw_largo_enviado = true;
            Enviar(ENC_CLICK_LARGO);
        }
    }
}

static void MuestrearEncoder(void *arg) {
    LeerGiro();
    LeerBoton();
}

/*==================[external functions definition]==========================*/
bool Encoder_Init(void) {
    cola = xQueueCreate(COLA_LARGO, sizeof(encoder_evento_t));
    if (cola == NULL) {
        return false;
    }

    GPIOInit(PIN_CLK, GPIO_INPUT);
    GPIOInit(PIN_DT, GPIO_INPUT);
    GPIOInit(PIN_SW, GPIO_INPUT);
    estado_ant = (GPIORead(PIN_CLK) << 1) | GPIORead(PIN_DT);

    const esp_timer_create_args_t args = {
        .callback = MuestrearEncoder,
        .name = "encoder",
    };
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) != ESP_OK) {
        return false;
    }
    return esp_timer_start_periodic(timer, PERIODO_US) == ESP_OK;
}

void Encoder_EsperarMantenido(uint32_t ms) {
    uint32_t apretado_ms = 0;
    while (apretado_ms < ms) {
        vTaskDelay(pdMS_TO_TICKS(10));
        apretado_ms = GPIORead(PIN_SW) ? 0 : apretado_ms + 10;   // activo en bajo
    }
    while (!GPIORead(PIN_SW)) {
        vTaskDelay(pdMS_TO_TICKS(10));   // espera a que lo suelte
    }
    xQueueReset(cola);
}

void Encoder_SetDobleClick(bool activar) {
    doble_habilitado = activar;
    if (!activar) {
        click_pendiente_ms = 0;
    }
}

bool Encoder_Leer(encoder_evento_t *evento, TickType_t espera) {
    return xQueueReceive(cola, evento, espera) == pdTRUE;
}
