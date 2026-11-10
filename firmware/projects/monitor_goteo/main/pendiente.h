#ifndef PENDIENTE_H
#define PENDIENTE_H
/** \addtogroup Pendiente Registro pendiente de descarga
 ** @{ */

/** \brief Seguimiento de la infusión terminada o interrumpida que todavía no se descargó con la app.
 *
 * Según el diseño del 10/11/2026 (ver README.md del proyecto):
 *
 * - Cuando termina la infusión (Detener monitoreo o Apagar), el registro queda **pendiente de descarga** y el contador de
 *   minutos desde el inicio **sigue corriendo** hasta que la app manda OK. La app calcula la hora de inicio como la hora
 *   del celular menos ese minuto. Cada minuto se guarda una muestra en el registro, así un reinicio no pierde la cuenta.
 * - Si el equipo se reinicia con una infusión sin terminar, **no la retoma**: la da por INTERRUMPIDA, anota REINICIO con
 *   el último minuto guardado y cuenta los minutos **desde el reinicio** hasta el OK. Mientras esté interrumpida y sin
 *   descargar, se activa la alarma EQUIPO_REINICIADO.
 * - Si el equipo se reinicia con un registro terminado y sin descargar, anota REINICIO y el contador sigue desde el último
 *   minuto guardado (el tiempo apagado se pierde, no hay RTC).
 * - Con el OK de la app se deja de contar y el último minuto queda congelado.
 *
 * El estado se guarda en la NVS (componente registro) junto con el ID de la última infusión.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 10/11/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>

/** @fn void Pendiente_Init(void)
 * @brief Lee de la NVS el estado de la última infusión (llamar después de Registro_Init)
 */
void Pendiente_Init(void);

/** @fn bool Pendiente_AlEncender(bool infusion_sin_terminar, int64_t ahora_us)
 * @brief Al encender: si había una infusión sin terminar la da por interrumpida, y si hay un registro pendiente anota
 * REINICIO (con el último minuto guardado) y empieza a contar.
 * @param infusion_sin_terminar true si la NVS tenía una infusión activa (se cortó la energía en pleno monitoreo)
 * @return true si hay un registro pendiente de descarga (hay que pedir la descarga)
 */
bool Pendiente_AlEncender(bool infusion_sin_terminar, int64_t ahora_us);

/** @fn void Pendiente_Terminada(uint32_t minuto_fin, int64_t ahora_us)
 * @brief La infusión terminó (se escribió el FIN): el registro queda pendiente y el minuto sigue contando desde minuto_fin
 */
void Pendiente_Terminada(uint32_t minuto_fin, int64_t ahora_us);

/** @fn void Pendiente_Tick(int64_t ahora_us)
 * @brief Llamar cada ~1 s: cada minuto que pasa escribe una muestra en el registro
 */
void Pendiente_Tick(int64_t ahora_us);

/** @fn void Pendiente_Descargado(int64_t ahora_us)
 * @brief La app confirmó la descarga (OK): deja de contar y congela el minuto
 */
void Pendiente_Descargado(int64_t ahora_us);

/** @fn void Pendiente_Descartar(void)
 * @brief Se empieza una infusión nueva ("Continuar igual") o se borró el registro: el registro pendiente queda de lado
 */
void Pendiente_Descartar(void);

/** @fn bool Pendiente_Hay(void)
 * @brief true si hay un registro (terminado o interrumpido) sin descargar
 */
bool Pendiente_Hay(void);

/** @fn bool Pendiente_EquipoReiniciado(void)
 * @brief true mientras haya una infusión interrumpida por un reinicio y sin descargar (condición de la alarma EQUIPO_REINICIADO)
 */
bool Pendiente_EquipoReiniciado(void);

/** @fn uint8_t Pendiente_Estado(void)
 * @brief Estado de la última infusión que no está en curso: 0 = terminada, 2 = interrumpida por un reinicio (REGISTRO_ESTADO_*)
 */
uint8_t Pendiente_Estado(void);

/** @fn uint32_t Pendiente_MinutoActual(int64_t ahora_us)
 * @brief Minuto que se informa por BLE (ESTADO y FIN_TX): terminada = minutos desde el inicio de la infusión; interrumpida =
 * minutos desde el reinicio. Mientras falte el OK sigue aumentando; después queda congelado.
 */
uint32_t Pendiente_MinutoActual(int64_t ahora_us);

/** @} */
#endif /* PENDIENTE_H */
