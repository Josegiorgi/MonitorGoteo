#ifndef DESCARGA_H
#define DESCARGA_H
/** \addtogroup Descarga Descarga del registro con la app
 ** @{ */

/** \brief Lo que el programa principal le da al Bluetooth y lo que hace cuando la app confirma la descarga.
 *
 * - Enlace con el componente comunicacion: le dice el estado de la última infusión y el minuto actual, y recibe el OK de la
 *   app (que corre en la tarea de comunicación, así que solo se anota y se atiende en el lazo principal).
 * - Registro pendiente de descarga (módulo pendiente): sigue contando minutos, atiende el OK de la app y el "Continuar igual"
 *   de la enfermera.
 *
 * Con USAR_BLE = 0 no hay Bluetooth: el enlace no existe, pero el registro pendiente sigue funcionando.
 */

#include <stdint.h>

/** @fn void Descarga_IniciarBle(void)
 * @brief Inicia el Bluetooth con la app (si USAR_BLE = 1). Llamar después del autotest, para que la radio no moleste la
 * medición del nivel base de los fotodiodos.
 */
void Descarga_IniciarBle(void);

/** @fn void Descarga_Atender(int64_t ahora_us)
 * @brief Registro pendiente de descarga: cuenta el minuto, atiende el OK de la app y el "Continuar igual" de la enfermera
 */
void Descarga_Atender(int64_t ahora_us);

/** @} */
#endif /* DESCARGA_H */
