#ifndef GOTEO_H
#define GOTEO_H
/** \addtogroup Sensado Sensado
 ** @{ */

/** \brief Cálculo de volumen, caudal y tiempo de la infusión a partir de las gotas detectadas.
 *
 * Recibe una llamada por cada gota que detecta el sensor óptico y, con el factor de goteo del
 * equipo, calcula lo que muestra la pantalla de uso:
 *
 * - Volumen infundido (mL) = gotas contadas / factor de goteo (gtt/mL).
 * - Caudal (mL/h) = 3600 / (segundos entre gotas x factor de goteo). El tiempo entre gotas se
 *   promedia con las últimas GOTEO_VENTANA gotas para que el valor no salte a cada gota.
 * - Tiempo (s) = transcurrido desde Goteo_Iniciar().
 * - Segundos sin gotas = desde la última gota, para la alarma de oclusión.
 *
 * El módulo no usa hardware ni relojes propios: todas las funciones reciben la hora actual en
 * microsegundos (por ejemplo esp_timer_get_time()). Así se puede probar con gotas simuladas.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 16/10/2026 | Document creation                              |
 *
 */

#include <stdint.h>

/*==================[macros]=================================================*/
#define GOTEO_VENTANA          8             /*!< Cantidad de gotas con las que se promedia el caudal */
#define GOTEO_CAUDAL_CERO_US   30000000LL    /*!< Si pasan más de 30 s sin gotas, el caudal es 0 */

/*==================[external functions declaration]=========================*/

/** @fn void Goteo_Iniciar(uint16_t factor_goteo, int64_t ahora_us)
 * @brief Empieza una infusión: pone en cero las gotas, el volumen y el tiempo.
 * @param factor_goteo Gotas por mL del equipo (10, 15, 20 o 60)
 * @param ahora_us Hora actual en microsegundos
 */
void Goteo_Iniciar(uint16_t factor_goteo, int64_t ahora_us);

/** @fn void Goteo_ReiniciarTiempo(int64_t ahora_us)
 * @brief Pone en cero solo el tiempo (y los segundos sin gotas), sin tocar las gotas ni el volumen.
 *
 * Sirve porque el volumen empieza a contarse apenas se abre el clamp (en el Ajuste de goteo ya
 * pasa líquido al paciente), pero el tiempo de la infusión empieza recién en el Monitoreo.
 */
void Goteo_ReiniciarTiempo(int64_t ahora_us);

/** @fn void Goteo_Gota(int64_t ahora_us)
 * @brief Registra una gota detectada. Llamar una vez por cada gota.
 * @param ahora_us Hora en que cayó la gota, en microsegundos
 */
void Goteo_Gota(int64_t ahora_us);

/** @fn uint32_t Goteo_Gotas(void)
 * @brief Gotas contadas desde Goteo_Iniciar()
 */
uint32_t Goteo_Gotas(void);

/** @fn uint16_t Goteo_VolumenMl(void)
 * @brief Volumen infundido en mL (gotas / factor de goteo, redondeado)
 */
uint16_t Goteo_VolumenMl(void);

/** @fn uint16_t Goteo_CaudalMlh(int64_t ahora_us)
 * @brief Caudal actual en mL/h, a partir del tiempo entre las últimas gotas.
 *
 * Si las gotas se espacian o se detienen, el valor baja solo (se usa el tiempo desde la última
 * gota si es mayor que el promedio). Con menos de 2 gotas, o más de GOTEO_CAUDAL_CERO_US sin
 * gotas, da 0.
 */
uint16_t Goteo_CaudalMlh(int64_t ahora_us);

/** @fn uint32_t Goteo_TiempoS(int64_t ahora_us)
 * @brief Segundos transcurridos desde Goteo_Iniciar()
 */
uint32_t Goteo_TiempoS(int64_t ahora_us);

/** @fn uint16_t Goteo_SegundosSinGotas(int64_t ahora_us)
 * @brief Segundos desde la última gota (o desde Goteo_Iniciar() si todavía no cayó ninguna)
 */
uint16_t Goteo_SegundosSinGotas(int64_t ahora_us);

/** @} */
#endif /* GOTEO_H */
