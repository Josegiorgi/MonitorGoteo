#ifndef MODO_TECNICO_H
#define MODO_TECNICO_H
/** \addtogroup Modo_Tecnico Modo Técnico
 ** @{ */

/** \brief Lo que el programa principal hace por pedido del Modo Técnico de la interfaz (se abre con el código técnico).
 *
 * La interfaz solo marca lo que se pidió en ui_datos_t (probar una alarma, borrar el registro, salir); acá se hace sonar
 * el buzzer, se borra la partición del registro y se reinicia el equipo. También carga los datos del Diagnóstico.
 */

/** @fn void ModoTecnico_Atender(void)
 * @brief Hace sonar la alarma que se eligió probar, borra el registro si se confirmó y sale (reinicia) si corresponde.
 * Llamar después de cada evento del encoder y en cada vuelta de 1 s.
 */
void ModoTecnico_Atender(void);

/** @fn void ModoTecnico_ActualizarDiagnostico(void)
 * @brief Carga en datos lo que muestra el Diagnóstico (solo lectura): nivel base de los fotodiodos, tensión de la batería,
 * líneas del registro y versión del firmware.
 */
void ModoTecnico_ActualizarDiagnostico(void);

/** @} */
#endif /* MODO_TECNICO_H */
