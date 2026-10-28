#ifndef UI_H
#define UI_H
/** \addtogroup Interfaz_Usuario Interfaz de usuario
 ** @{ */

/** \brief Pantallas de la OLED (u8g2, SSD1315 128x64 por I2C) y navegación con el encoder.
 *
 * El módulo muestra los datos que le carga el main (ui_datos_t) y maneja la navegación entre
 * pantallas con los eventos del encoder: giro = mover o cambiar, click = "Pulsar",
 * click largo = "Mantener" y doble click (solo en Monitoreo, alarma y fin de infusión). No mide nada ni decide las alarmas: el main (con el módulo de
 * sensado) completa ui_datos_t, incluida la alarma activa, y la interfaz la muestra.
 *
 * Flujo de pantallas:
 *
 * Inicio (autotest) -> Autotest -> Colocación -> Config 1/3..3/3 -> Resumen -> Ajuste de
 * goteo -> Monitoreo. La primera configuración no pide código. Una vez en uso, el Acceso
 * técnico (código de 4 dígitos) es la única forma de cambiar parámetros o apagar el equipo,
 * para que el usuario o el cuidador no puedan alterar la infusión: desde el Monitoreo (o desde
 * Fin de infusión), el doble click abre el Acceso técnico y de ahí el Menú técnico (Configurar o
 * Apagar equipo). Una alarma interrumpe el Monitoreo con su pantalla; la de colocación vuelve
 * a la pantalla de Colocación.
 *
 * @note I2C: SCL = GPIO_7, SDA = GPIO_21.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 15/10/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "u8g2.h"
#include "encoder.h"

/*==================[macros]=================================================*/
#define UI_CODIGO_TECNICO   "1234"  /*!< Código de 4 dígitos del Acceso técnico (provisorio) */
#define UI_CHEQUEOS_MAX     4       /*!< Cantidad máxima de chequeos del autotest */

/*==================[typedef]================================================*/
/**
 * @brief Alarmas que puede mostrar la interfaz
 */
typedef enum {
    UI_ALARMA_NINGUNA = 0,
    UI_ALARMA_OCLUSION,           /*!< No se detectan gotas durante un tiempo configurable (flujo detenido) */
    UI_ALARMA_DESVIACION_FLUJO,   /*!< El caudal real se desvía más del 10 % del objetivo (por arriba o por debajo) */
    UI_ALARMA_FIN_SACHET,         /*!< Se alcanzó el volumen programado y ya no hay gotas */
    UI_ALARMA_BATERIA_BAJA,       /*!< Carga de la batería en estado crítico */
    UI_ALARMA_COLOCACION,         /*!< El equipo se movió de la guía (p. ej. por un choque): vuelve a la pantalla de colocación */
} ui_alarma_t;

/**
 * @brief Datos que muestra la interfaz. Los completa el main; la interfaz solo modifica
 * los de configuración (cuando se editan en pantalla) y alarma_silenciada, en_pausa e
 * infusion_configurada.
 */
typedef struct {
    // Configuración de la infusión (se edita en Config)
    uint16_t factor_goteo;         /*!< Gotas por mL del equipo (gtt/mL): 10, 15, 20 o 60 */
    uint16_t volumen_total_ml;     /*!< Volumen programado, en mL: 50, 100, 250, 500 o 1000 */
    uint16_t duracion_min;         /*!< Tiempo total programado, en minutos: de 10 min a 24 h, de a 10 min (se muestra como h:mm) */
    uint16_t flujo_objetivo_mlh;   /*!< Caudal objetivo en mL/h: lo calcula el equipo (volumen / tiempo), no se carga */
    uint16_t umbral_sin_gotas_s;   /*!< Segundos sin gotas para dar la alarma de oclusión */

    // Medición (los completa el sensado)
    uint16_t flujo_mlh;            /*!< Caudal medido, en mL/h */
    uint16_t volumen_ml;           /*!< Volumen infundido, en mL */
    uint32_t tiempo_s;             /*!< Tiempo de infusión, en segundos: la interfaz lo pone en 0 al empezar el Monitoreo (después del Ajuste de goteo); el main lo hace avanzar */
    uint16_t segundos_sin_gotas;   /*!< Segundos que lleva sin detectar gotas */
    float    angulo_grados;        /*!< Ángulo del equipo respecto de la vertical */
    bool     colocacion_ok;        /*!< true si el ángulo está dentro de la tolerancia */
    float    tolerancia_grados;    /*!< Rango correcto del ángulo (+/- este valor); la pantalla de colocación lo muestra */
    uint8_t  bateria_pct;          /*!< Carga de la batería, 0 a 100 */

    // Estado
    ui_alarma_t alarma;            /*!< Alarma activa (UI_ALARMA_NINGUNA si no hay) */
    bool alarma_silenciada;        /*!< true si se la silenció con "Pulsar" (queda un aviso en el Monitoreo) */
    bool en_pausa;                 /*!< true si la infusión está pausada */
    bool infusion_configurada;     /*!< true desde que se confirma el Resumen hasta que finaliza */
} ui_datos_t;

/**
 * @brief Resultado de un chequeo del autotest
 */
typedef enum {
    UI_CHEQUEO_PENDIENTE,   /*!< Todavía no se chequeó (se muestra "..") */
    UI_CHEQUEO_OK,          /*!< Respondió bien (se muestra "OK") */
    UI_CHEQUEO_FALLA,       /*!< No respondió (se muestra "ERR") */
} ui_chequeo_estado_t;

/**
 * @brief Un ítem del autotest
 */
typedef struct {
    const char *nombre;         /*!< Nombre corto, hasta 4 letras (p. ej. "OLED") */
    ui_chequeo_estado_t estado; /*!< Resultado actual */
} ui_chequeo_t;

/**
 * @brief Pantallas a las que se navega con el encoder (el Inicio es aparte: UI_PantallaInicio)
 */
typedef enum {
    UI_PANTALLA_AUTOTEST,       /*!< Resumen de los chequeos. Pulsar: seguir */
    UI_PANTALLA_COLOCACION,     /*!< Ángulo del equipo en la guía. Pulsar: seguir (solo si es correcto) */
    UI_PANTALLA_ACCESO,         /*!< Acceso técnico: código de 4 dígitos */
    UI_PANTALLA_CONFIG,         /*!< Un parámetro por pantalla: factor de goteo, volumen y tiempo total */
    UI_PANTALLA_RESUMEN,        /*!< Confirmación de la configuración. Pulsar: iniciar */
    UI_PANTALLA_AJUSTE_GOTEO,   /*!< Informa las gotas/min objetivo para regular el clamp; la enfermera lo hace sola. Pulsar: seguir */
    UI_PANTALLA_MONITOREO,      /*!< Caudal, volumen y tiempo (la pantalla "En uso") */
    UI_PANTALLA_ALARMA,         /*!< Alarma activa. Pulsar: silenciar */
    UI_PANTALLA_MENU_TECNICO,   /*!< Configurar y Apagar equipo */
    UI_PANTALLA_FIN,            /*!< Infusión finalizada. Doble click: menú */
    UI_PANTALLA_APAGAR,         /*!< Confirmar apagado */
} ui_pantalla_t;

/*==================[external functions declaration]=========================*/

/** @fn bool UI_Init(void)
 * @brief Inicia el I2C y la OLED y la deja en blanco.
 * @return true siempre (la OLED no informa errores al inicializar)
 */
bool UI_Init(void);

/** @fn void UI_PantallaInicio(uint8_t cuadro)
 * @brief Dibuja la pantalla de inicio ("Iniciando" con una animación de espera)
 * @param cuadro Número de cuadro de la animación (28 cuadros, se repite); en Lopaka avanza cada 42 ms
 */
void UI_PantallaInicio(uint8_t cuadro);

/** @fn void UI_SetChequeos(const ui_chequeo_t *chequeos, uint8_t cantidad)
 * @brief Carga los resultados que muestra la pantalla Autotest (la lista debe seguir existiendo)
 */
void UI_SetChequeos(const ui_chequeo_t *chequeos, uint8_t cantidad);

/** @fn void UI_Ir(ui_pantalla_t destino, ui_datos_t *datos)
 * @brief Salta a una pantalla y la dibuja (p. ej. UI_PANTALLA_AUTOTEST al terminar el Inicio)
 */
void UI_Ir(ui_pantalla_t destino, ui_datos_t *datos);

/** @fn void UI_Evento(encoder_evento_t evento, ui_datos_t *datos)
 * @brief Maneja un evento del encoder según la pantalla actual y la redibuja.
 * @param evento Evento sacado de Encoder_Leer()
 * @param datos Datos compartidos con el resto del sistema (la interfaz puede modificarlos)
 */
void UI_Evento(encoder_evento_t evento, ui_datos_t *datos);

/** @fn void UI_Actualizar(ui_datos_t *datos)
 * @brief Llamar cada ~1 s: pasa a la pantalla de una alarma que se activó o vuelve al
 * Monitoreo si se apagó, hace parpadear la alarma y redibuja con los datos nuevos.
 */
void UI_Actualizar(ui_datos_t *datos);

/** @fn ui_pantalla_t UI_PantallaActual(void)
 * @brief Pantalla en la que está la interfaz
 */
ui_pantalla_t UI_PantallaActual(void);

/** @fn bool UI_ApagarPedido(void)
 * @brief true si el usuario confirmó "Apagar equipo? Si". El main debe apagar el equipo.
 */
bool UI_ApagarPedido(void);

/** @fn void UI_PantallaApagada(void)
 * @brief Deja la OLED apagada (modo de bajo consumo).
 */
void UI_PantallaApagada(void);

/** @} */
#endif /* UI_H */
