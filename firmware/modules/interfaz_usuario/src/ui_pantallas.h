#ifndef UI_PANTALLAS_H
#define UI_PANTALLAS_H
/* Funciones de dibujo de las pantallas (uso interno del módulo). Cada una borra el buffer,
 * dibuja y lo envía a la OLED; la navegación y los datos están en ui.c. */

#include <stdint.h>
#include <stdbool.h>
#include "u8g2.h"
#include "ui.h"

/** Autotest: resultados de los chequeos y "Pulsar: seguir" */
void Pant_Autotest(u8g2_t *g, uint8_t bateria_pct, const ui_chequeo_t *chequeos, uint8_t cantidad, bool todos_ok);

/** Acceso clínico: dígitos ya aceptados ('*'), el que se está eligiendo y los que faltan ('_') */
void Pant_Acceso(u8g2_t *g, const uint8_t *digitos, uint8_t posicion, bool error);

/** Lista con 3 filas visibles, cursor resaltado y barra de desplazamiento */
void Pant_Menu(u8g2_t *g, const char *titulo, const char *const *items, uint8_t cantidad, uint8_t cursor, uint8_t primero, const char *pie);

/** Parámetro de configuración: nombre, valor entre "< >" y unidad */
void Pant_Config(u8g2_t *g, uint8_t numero, uint8_t total, const char *nombre, const char *valor, const char *unidad);

/** Resumen de la configuración antes de iniciar: factor de goteo, volumen y tiempo total */
void Pant_Resumen(u8g2_t *g, const ui_datos_t *d);

/** Ajuste de goteo: informa las gotas/min para regular el clamp y el cálculo de donde salen
 * (volumen, tiempo y factor de goteo que cargó la enfermera) */
void Pant_AjusteGoteo(u8g2_t *g, uint16_t gotas_min, uint16_t volumen_ml, uint16_t duracion_min, uint16_t factor_goteo);

/** Alarma activa; invertida=true la dibuja en blanco con texto negro (parpadea alternando) */
void Pant_Alarma(u8g2_t *g, bool invertida, const char *nivel, const char *titulo, const char *detalle);

/** Infusión finalizada: volumen y tiempo total */
void Pant_Fin(u8g2_t *g, uint8_t bateria_pct, uint16_t volumen_ml, uint32_t tiempo_s);

/** Confirmar apagado: si_elegido=true resalta "Si", si no resalta "No" */
void Pant_Apagar(u8g2_t *g, bool si_elegido);

#endif /* UI_PANTALLAS_H */
