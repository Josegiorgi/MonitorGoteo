#ifndef AUTOTEST_H
#define AUTOTEST_H
/** \addtogroup Autotest Autotest de arranque
 ** @{ */

/** \brief Los cinco chequeos que se hacen al encender: pantalla, acelerómetro, sensor óptico, memoria y batería.
 *
 * Se hacen mientras se muestra la pantalla de inicio. Un chequeo que falla queda como ERR, se reintenta cada segundo y,
 * si hay que bloquear, la pantalla Autotest dice cuál falló y qué hacer. Si falla la pantalla misma no hay cómo mostrar un
 * texto: avisa con el buzzer hasta que responda.
 */

#include <stdbool.h>

/** @fn void Autotest_Init(bool oled_ok, bool sensor_ok, bool acel_ok)
 * @brief Le informa al autotest cómo quedaron la pantalla, el sensor óptico y el acelerómetro al iniciarlos, y le pasa a la
 * interfaz la lista de chequeos. La memoria y la batería se vuelven a probar en el autotest.
 */
void Autotest_Init(bool oled_ok, bool sensor_ok, bool acel_ok);

/** @fn void Autotest_Ejecutar(bool bloquear)
 * @brief Hace el autotest: la pantalla "Iniciando" queda con la animación esperando a que todos los chequeos den OK, y como
 * mínimo DURACION_INICIO_MS.
 * @param bloquear true: no sigue hasta que todos den OK. false: sigue igual (con un registro pendiente de descarga no hay una
 * enfermera para leer la falla y tiene que sonar la alarma de reinicio)
 */
void Autotest_Ejecutar(bool bloquear);

/** @} */
#endif /* AUTOTEST_H */
