#ifndef REPOSO_H
#define REPOSO_H
/** \addtogroup Reposo Reposo y reinicio a propósito
 ** @{ */

/** \brief Reposo después de Apagar y reinicio sin volver a pedir el botón 3 s.
 *
 * - Reposo: pantalla en negro, sensado y buzzer apagados, y el micro en light sleep. Despierta con el botón: si se mantiene
 *   3 s reinicia el equipo (logo, autotest y clave); con menos vuelve a dormir sin encender la pantalla.
 * - No es consumo cero: el LED infrarrojo sigue encendido por hardware y el divisor de la batería consume ~210 uA; para
 *   guardar el equipo hay que abrir el interruptor interno.
 * - Reinicio a propósito (después del reposo o de Salir del Modo Técnico): deja una marca en una memoria que sobrevive al
 *   reinicio para que al arrancar no se pida de nuevo el botón 3 s.
 */

#include <stdbool.h>

#define ENCENDIDO_MS   3000   /*!< Tiempo que hay que mantener apretado el botón para encender */

/** @fn void Reposo_Entrar(void)
 * @brief Pasa a reposo. No vuelve: al despertar con el botón mantenido reinicia el equipo.
 */
void Reposo_Entrar(void);

/** @fn void Reposo_ReiniciarSinEspera(void)
 * @brief Reinicia el equipo dejando la marca para no pedir de nuevo el botón 3 s
 */
void Reposo_ReiniciarSinEspera(void);

/** @fn bool Reposo_ReinicioAProposito(void)
 * @brief true si este arranque viene de un reinicio a propósito. Borra la marca: vale una sola vez.
 */
bool Reposo_ReinicioAProposito(void);

/** @} */
#endif /* REPOSO_H */
