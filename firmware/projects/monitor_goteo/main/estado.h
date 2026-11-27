#ifndef ESTADO_H
#define ESTADO_H
/** \addtogroup Estado Estado compartido del programa principal
 ** @{ */

/** \brief Lo único que comparten los módulos del programa principal.
 *
 * - datos: lo que muestra la interfaz (volumen, caudal, alarma, batería, ángulo, pedidos del Modo Técnico...). Lo completan
 *   los módulos del programa principal y lo dibuja interfaz_usuario.
 * - midiendo: true desde que empieza el Ajuste de goteo hasta que termina la infusión (se cuentan las gotas).
 *
 * Los dos se definen en estado.c. Todo lo demás que necesita un módulo es suyo y se pide con sus funciones.
 */

#include <stdbool.h>
#include "ui.h"

extern ui_datos_t datos;   /*!< Datos que se muestran en la interfaz */
extern bool midiendo;      /*!< true mientras se cuentan las gotas de una infusión */

/** @} */
#endif /* ESTADO_H */
