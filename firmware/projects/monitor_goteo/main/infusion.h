#ifndef INFUSION_H
#define INFUSION_H
/** \addtogroup Infusion Infusión en curso
 ** @{ */

/** \brief Ciclo de vida de la infusión: de las gotas que llegan del sensor al registro de lo que ocurre.
 *
 * - Recibe las gotas de drop_detection por una cola (Infusion_AvisoGota corre en la tarea del sensor y solo guarda la hora)
 *   y las cuenta (módulo sensado) y las pasa a las alarmas.
 * - Sigue los cambios de pantalla de la interfaz para saber cuándo empieza la medición (al entrar al Ajuste de goteo), cuándo
 *   arranca el tiempo (al confirmar el ajuste) y cuándo termina la infusión (Detener monitoreo o Apagar).
 * - Escribe en el registro lo que ocurre: inicio, ajuste, cambio de datos, fin, avisos y alarmas, silenciados, claves y la
 *   muestra de cada minuto, y guarda en la NVS la infusión activa.
 * - Recuerda en el Ajuste de goteo que falta confirmar.
 */

#include <stdint.h>
#include <stdbool.h>
#include "ui.h"
#include "alarmas.h"

/** @fn void Infusion_Init(void)
 * @brief Crea la cola de gotas. Llamar antes de iniciar el detector (DropDetection_Init).
 */
void Infusion_Init(void);

/** @fn void Infusion_AvisoGota(int64_t ahora_us)
 * @brief Función de aviso de drop_detection (corre en la tarea del sensor): solo guarda la hora de la gota
 */
void Infusion_AvisoGota(int64_t ahora_us);

/** @fn int Infusion_ProcesarGotas(void)
 * @brief Saca de la cola las gotas detectadas y las cuenta (con infusión) o las pasa al goteo sin monitoreo.
 * @return Cuántas gotas se contaron en la infusión
 */
int Infusion_ProcesarGotas(void);

/** @fn void Infusion_ActualizarMedicion(int64_t ahora_us)
 * @brief Carga en datos el volumen, el caudal, el tiempo y los segundos sin gotas
 */
void Infusion_ActualizarMedicion(int64_t ahora_us);

/** @fn void Infusion_RevisarTransiciones(ui_pantalla_t antes)
 * @brief Mira si la interfaz cambió de pantalla (antes era "antes") y arranca lo que corresponda: empezar la medición, retomar el
 * tiempo, cambiar la duración o terminar la infusión.
 */
void Infusion_RevisarTransiciones(ui_pantalla_t antes);

/** @fn void Infusion_GuardarMuestraSiToca(int64_t ahora_us)
 * @brief Con la infusión en curso, escribe en el registro la muestra de cada GUARDAR_CADA_S segundos
 */
void Infusion_GuardarMuestraSiToca(int64_t ahora_us);

/** @fn void Infusion_RecordarAjuste(int64_t ahora_us)
 * @brief Si pasó mucho tiempo en el Ajuste de goteo sin confirmar, el pie lo recuerda y suena un beep breve cada tanto
 */
void Infusion_RecordarAjuste(int64_t ahora_us);

/** @fn bool Infusion_SinInfusionEvaluable(void)
 * @brief true si no hay infusión activa y se vigila que no caigan gotas (pantalla Sin infusión y clave del arranque)
 */
bool Infusion_SinInfusionEvaluable(void);

/** @fn bool Infusion_EnCurso(void)
 * @brief true si hay una infusión en curso (configurada y midiendo)
 */
bool Infusion_EnCurso(void);

/** @fn uint16_t Infusion_Minuto(int64_t ahora_us)
 * @brief Minutos desde el inicio de la infusión (0 si todavía no empezó el tiempo)
 */
uint16_t Infusion_Minuto(int64_t ahora_us);

/** @fn void Infusion_EventoAlarma(alarma_evento_t evento, bool activo, int64_t ahora_us)
 * @brief Función de aviso de alarmas (Alarmas_SetCallback): cada aviso o alarma que se activa o se resuelve queda en el registro
 */
void Infusion_EventoAlarma(alarma_evento_t evento, bool activo, int64_t ahora_us);

/** @fn void Infusion_AnotarSilenciada(int evento, int64_t ahora_us)
 * @brief Anota en el registro que se silenció una alarma
 */
void Infusion_AnotarSilenciada(int evento, int64_t ahora_us);

/** @fn void Infusion_EventoClave(ui_clave_resultado_t resultado)
 * @brief Función de aviso de la interfaz (UI_SetCallbackClave): cada clave ingresada queda en el registro
 */
void Infusion_EventoClave(ui_clave_resultado_t resultado);

/** @} */
#endif /* INFUSION_H */
