#ifndef UI_PANTALLAS_H
#define UI_PANTALLAS_H
/* Funciones de dibujo de las pantallas (uso interno del módulo). Cada una borra el buffer,
 * dibuja y lo envía a la OLED; la navegación y los datos están en ui.c. */

#include <stdint.h>
#include <stdbool.h>
#include "u8g2.h"
#include "ui.h"

/** Autotest: solo se muestra si un chequeo falla: "Falla: <nombre>" y qué hacer ("No usar", "Conectar cargador") */
void Pant_Autotest(u8g2_t *g, uint8_t bateria_pct, const ui_chequeo_t *chequeos, uint8_t cantidad, bool todos_ok);

/** Acceso clínico: dígitos ya aceptados ('*'), el que se está eligiendo y los que faltan ('_') */
void Pant_Acceso(u8g2_t *g, const uint8_t *digitos, uint8_t posicion, bool error, uint16_t bloqueo_s);

/** Lista con 3 filas visibles, cursor resaltado y barra de desplazamiento */
void Pant_Menu(u8g2_t *g, const char *titulo, const char *const *items, uint8_t cantidad, uint8_t cursor, uint8_t primero, const char *pie);

/** Parámetro de configuración: nombre, valor entre "< >" y unidad */
void Pant_Config(u8g2_t *g, uint8_t numero, uint8_t total, const char *nombre, const char *valor, const char *unidad);

/** Resumen de la configuración antes de iniciar, en dos páginas: 0 = factor de goteo destacado con la leyenda
 * "Verificar en el envase de la guía"; 1 = volumen, duración, mL/h y gtt/min. motivo = 0 si se puede iniciar,
 * 1 si el objetivo es menor a 2 gotas/min, 2 si no queda lugar en la memoria del registro: avisa y no se puede iniciar */
void Pant_Resumen(u8g2_t *g, const ui_datos_t *d, uint8_t pagina, uint8_t motivo);

/** Ajuste de goteo: informa las gotas/min para regular el clamp y el cálculo de donde salen
 * (volumen, tiempo y factor de goteo que cargó la enfermera) */
void Pant_AjusteGoteo(u8g2_t *g, uint16_t gotas_min, uint16_t volumen_ml, uint16_t duracion_min, uint16_t factor_goteo, bool recordar);

/** Alarma activa; invertida=true la dibuja en blanco con texto negro (parpadea alternando) */
void Pant_Alarma(u8g2_t *g, bool invertida, const char *nivel, const char *derecha, const char *titulo, const char *detalle);

/** Sin infusión activa (bloqueada: para operar hay que mantener el botón y poner la clave) */
void Pant_SinInfusion(u8g2_t *g, uint8_t bateria_pct);

/** Mensaje corto centrado (p. ej. "Registro guardado") */
void Pant_Mensaje(u8g2_t *g, const char *texto);

/** Modo Técnico > Diagnóstico: nivel base del sensor óptico, ángulo, batería, registro y versión (solo lectura) */
void Pant_Diagnostico(u8g2_t *g, const ui_datos_t *d);

/** Confirmación con dos líneas de texto y No / Si: si_elegido=true resalta "Si", si no resalta "No" */
void Pant_Confirmar(u8g2_t *g, const char *titulo, const char *linea1, const char *linea2, bool si_elegido);

#endif /* UI_PANTALLAS_H */
