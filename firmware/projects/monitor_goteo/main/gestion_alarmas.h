#ifndef GESTION_ALARMAS_H
#define GESTION_ALARMAS_H
/** \addtogroup Gestion_Alarmas Gestión de alarmas
 ** @{ */

/** \brief Une el componente alarmas con el resto del equipo.
 *
 * El componente alarmas decide qué evento está activo a partir de lo que se le informa (gotas, ángulo, batería...); acá se
 * reúnen esos datos cada segundo, se pasa a la interfaz la alarma que corresponde mostrar y se la hace sonar en el buzzer.
 * También se leen el ángulo (acelerómetro) y la carga de la batería, y se atiende el click que silencia una alarma.
 */

#include <stdint.h>
#include <stdbool.h>

/** @fn void GestionAlarmas_ActualizarAngulo(void)
 * @brief Carga en datos el ángulo del equipo y si la colocación es correcta. Si el acelerómetro no responde, la colocación no
 * se puede verificar: se da por incorrecta en vez de ignorarlo.
 */
void GestionAlarmas_ActualizarAngulo(void);

/** @fn void GestionAlarmas_ActualizarBateria(void)
 * @brief Carga en datos la carga de la batería. Sin batería conectada queda en 100 %, para que el equipo conectado solo al USB no
 * dé alarmas de batería.
 */
void GestionAlarmas_ActualizarBateria(void);

/** @fn void GestionAlarmas_Evaluar(int64_t ahora_us, bool forzar)
 * @brief Una vez por segundo: evalúa las alarmas, las pasa a la interfaz y las hace sonar.
 * @param forzar true para evaluar aunque la pantalla no sea de "sin infusión" (apaga una alarma cuando llega el OK o se
 * descarta el registro)
 */
void GestionAlarmas_Evaluar(int64_t ahora_us, bool forzar);

/** @fn void GestionAlarmas_Silenciar(int64_t ahora_us)
 * @brief La enfermera silenció la alarma que se mostraba: se anota en el registro, deja de sonar y pasa la siguiente, si hay.
 */
void GestionAlarmas_Silenciar(int64_t ahora_us);

/** @} */
#endif /* GESTION_ALARMAS_H */
