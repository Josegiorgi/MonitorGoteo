#ifndef ENCODER_H
#define ENCODER_H
/** \addtogroup Interfaz_Usuario Interfaz de usuario
 ** @{ */

/** \brief Encoder rotativo de 5 patas (CLK, DT, SW, VCC, GND).
 *
 * Lee los tres pines por muestreo cada 1 ms (sin interrupciones, el muestreo ya
 * actúa como antirrebote) y entrega eventos por una cola.
 *
 * @note Pines: CLK = GPIO_9, DT = GPIO_8, SW = GPIO_20. Usan el pull-up interno.
 * @note Si el sentido de giro sale invertido, cambiar ENCODER_INVERTIR_SENTIDO a 1.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 15/10/2026 | Document creation		                         |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

/*==================[macros]=================================================*/
#define ENCODER_INVERTIR_SENTIDO  1    /*!< 1 intercambia horario/antihorario (con el cableado actual girar a la derecha salía invertido) */
#define ENCODER_CLICK_LARGO_MS    800  /*!< Tiempo apretado para que sea click largo */
#define ENCODER_DOBLE_CLICK_MS    350  /*!< Tiempo máximo entre dos clicks para que cuenten como doble click */

/*==================[typedef]================================================*/
/**
 * @brief Eventos que entrega el encoder
 */
typedef enum {
    ENC_GIRO_HORARIO = 1,   /*!< Un paso (un "clic" del eje) en sentido horario */
    ENC_GIRO_ANTIHORARIO,   /*!< Un paso en sentido antihorario */
    ENC_CLICK,              /*!< Botón apretado y soltado rápido */
    ENC_CLICK_LARGO,        /*!< Botón mantenido ENCODER_CLICK_LARGO_MS */
    ENC_DOBLE_CLICK,        /*!< Dos clicks seguidos (solo si se activó con Encoder_SetDobleClick) */
} encoder_evento_t;

/*==================[external functions declaration]=========================*/

/** @fn bool Encoder_Init(void)
 * @brief Configura los pines, crea la cola de eventos y arranca el muestreo.
 * @return true si quedó funcionando
 */
bool Encoder_Init(void);

/** @fn void Encoder_SetDobleClick(bool activar)
 * @brief Activa o desactiva la detección de doble click.
 *
 * Activada, un click simple se entrega recién ENCODER_DOBLE_CLICK_MS después (hay que esperar
 * para saber si viene un segundo). Por eso la interfaz la activa solo en las pantallas que la
 * usan (Monitoreo, alarma y fin de infusión) y no demora los clicks del resto.
 */
void Encoder_SetDobleClick(bool activar);

/** @fn bool Encoder_Leer(encoder_evento_t *evento, TickType_t espera)
 * @brief Saca un evento de la cola.
 * @param evento Donde se guarda el evento
 * @param espera Ticks que se espera si no hay eventos (0 no espera, portMAX_DELAY espera siempre)
 * @return true si había un evento
 */
bool Encoder_Leer(encoder_evento_t *evento, TickType_t espera);

/** @fn void Encoder_EsperarMantenido(uint32_t ms)
 * @brief Bloquea hasta que el botón se mantenga apretado ms milisegundos seguidos (para
 * "encender" el equipo). Después espera a que se suelte y descarta los eventos acumulados,
 * así el click largo de ese apretón no llega a la interfaz.
 * @param ms Tiempo que hay que mantener apretado el botón
 * @note Llamar después de Encoder_Init().
 */
void Encoder_EsperarMantenido(uint32_t ms);

/** @} */
#endif /* ENCODER_H */
