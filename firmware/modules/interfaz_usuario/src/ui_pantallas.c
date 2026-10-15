/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <string.h>
#include "ui_pantallas.h"

/*==================[macros and definitions]=================================*/
#define ANCHO          128
#define ANCHO_UTIL     124   /*!< Ancho máximo de un texto centrado, con 2 px de margen a cada lado */
#define Y_LINEA_CAB    14    /*!< Línea debajo de la cabecera */
#define Y_LINEA_PIE    52    /*!< Línea encima del pie */

/*==================[internal functions definition]==========================*/
static void Preparar(u8g2_t *g) {
    u8g2_ClearBuffer(g);
    u8g2_SetFontMode(g, 1);
    u8g2_SetBitmapMode(g, 1);
    u8g2_SetDrawColor(g, 1);
}

static uint8_t Centrar(u8g2_t *g, const char *texto) {
    return (ANCHO - u8g2_GetStrWidth(g, texto)) / 2;
}

/* Texto centrado en y; si no entra con la primera fuente usa la segunda */
static void TextoCentrado(u8g2_t *g, uint8_t y, const char *texto, const uint8_t *fuente, const uint8_t *fuente_chica) {
    u8g2_SetFont(g, fuente);
    if (u8g2_GetStrWidth(g, texto) > ANCHO_UTIL) {
        u8g2_SetFont(g, fuente_chica);
    }
    u8g2_DrawStr(g, Centrar(g, texto), y, texto);
}

static void Cabecera(u8g2_t *g, const char *izquierda, const char *derecha) {
    u8g2_SetFont(g, u8g2_font_6x13_tr);
    u8g2_DrawStr(g, 2, 11, izquierda);
    if (derecha != NULL) {
        u8g2_DrawStr(g, ANCHO - 2 - u8g2_GetStrWidth(g, derecha), 11, derecha);
    }
    u8g2_DrawHLine(g, 0, Y_LINEA_CAB, ANCHO);
}

static void Pie(u8g2_t *g, const char *texto) {
    u8g2_DrawHLine(g, 0, Y_LINEA_PIE, ANCHO);
    TextoCentrado(g, 62, texto, u8g2_font_6x10_tr, u8g2_font_5x7_tr);
}

/*==================[external functions definition]==========================*/
void Pant_Autotest(u8g2_t *g, uint8_t bateria_pct, const ui_chequeo_t *chequeos, uint8_t cantidad, bool todos_ok) {
    static const char *const estados[] = {"..", "OK", "ERR"};
    char texto[24];

    Preparar(g);
    snprintf(texto, sizeof(texto), "BAT %u%%", bateria_pct);
    Cabecera(g, texto, NULL);

    // Chequeos en dos columnas de dos filas
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    for (uint8_t i = 0; i < cantidad && i < UI_CHEQUEOS_MAX; i++) {
        snprintf(texto, sizeof(texto), "%-4s %s", chequeos[i].nombre, estados[chequeos[i].estado]);
        u8g2_DrawStr(g, (i % 2) ? 68 : 6, (i / 2) ? 44 : 30, texto);
    }
    Pie(g, todos_ok ? "Pulsar: seguir" : "Esperando...");
    u8g2_SendBuffer(g);
}

void Pant_Acceso(u8g2_t *g, const uint8_t *digitos, uint8_t posicion, bool error) {
    const uint8_t ancho = 20, alto = 24, separacion = 8, x0 = 12, y0 = 20;
    char caracter[2] = {0, 0};

    Preparar(g);
    Cabecera(g, "Acceso tecnico", NULL);
    u8g2_SetFont(g, u8g2_font_10x20_tr);

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t x = x0 + i * (ancho + separacion);
        if (i == posicion) {
            // Dígito que se está eligiendo: caja blanca con el número en negro
            u8g2_DrawBox(g, x, y0, ancho, alto);
            u8g2_SetDrawColor(g, 0);
            caracter[0] = '0' + digitos[i];
        } else {
            u8g2_DrawFrame(g, x, y0, ancho, alto);
            caracter[0] = (i < posicion) ? '*' : '_';
        }
        u8g2_DrawStr(g, x + (ancho - u8g2_GetStrWidth(g, caracter)) / 2, y0 + 20, caracter);
        u8g2_SetDrawColor(g, 1);
    }
    Pie(g, error ? "Codigo incorrecto" : "Giro: +/- Pulsar: OK");
    u8g2_SendBuffer(g);
}

void Pant_Menu(u8g2_t *g, const char *titulo, const char *const *items, uint8_t cantidad, uint8_t cursor, uint8_t primero, const char *pie) {
    const uint8_t filas = 3, alto_fila = 11, y0 = 16, alto_total = filas * alto_fila + 1;

    Preparar(g);
    Cabecera(g, titulo, NULL);
    u8g2_SetFont(g, u8g2_font_6x10_tr);

    for (uint8_t i = 0; i < filas && primero + i < cantidad; i++) {
        uint8_t y = y0 + i * alto_fila;
        if (primero + i == cursor) {
            u8g2_DrawBox(g, 0, y, 122, alto_fila);
            u8g2_SetDrawColor(g, 0);
        }
        u8g2_DrawStr(g, 4, y + 9, items[primero + i]);
        u8g2_SetDrawColor(g, 1);
    }

    // Barra de desplazamiento a la derecha, solo si la lista no entra completa
    if (cantidad > filas) {
        uint8_t alto_barra = alto_total * filas / cantidad;
        if (alto_barra < 4) {
            alto_barra = 4;
        }
        uint8_t y_barra = y0 + (alto_total - alto_barra) * primero / (cantidad - filas);
        u8g2_DrawVLine(g, 126, y0, alto_total);
        u8g2_DrawBox(g, 124, y_barra, 4, alto_barra);
    }
    Pie(g, pie);
    u8g2_SendBuffer(g);
}

void Pant_Config(u8g2_t *g, uint8_t numero, uint8_t total, const char *nombre, const char *valor, const char *unidad) {
    char texto[24];
    uint8_t ancho_valor, ancho_unidad, x;

    Preparar(g);
    snprintf(texto, sizeof(texto), "%u/%u", numero, total);
    Cabecera(g, "Config", texto);

    u8g2_SetFont(g, u8g2_font_6x10_tr);
    u8g2_DrawStr(g, Centrar(g, nombre), 28, nombre);
    ancho_unidad = u8g2_GetStrWidth(g, unidad);

    // "< valor >" grande seguido de la unidad, todo el conjunto centrado
    snprintf(texto, sizeof(texto), "< %s >", valor);
    u8g2_SetFont(g, u8g2_font_10x20_tr);
    ancho_valor = u8g2_GetStrWidth(g, texto);
    x = (ANCHO - (ancho_valor + 4 + ancho_unidad)) / 2;
    u8g2_DrawStr(g, x, 46, texto);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    u8g2_DrawStr(g, x + ancho_valor + 4, 46, unidad);

    Pie(g, "Giro: cambiar Pulsar: OK");
    u8g2_SendBuffer(g);
}

void Pant_Resumen(u8g2_t *g, const ui_datos_t *d) {
    char texto[24];

    Preparar(g);
    Cabecera(g, "Confirmar", NULL);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    snprintf(texto, sizeof(texto), "FG  %u gtt/mL", d->factor_goteo);
    u8g2_DrawStr(g, 6, 27, texto);
    snprintf(texto, sizeof(texto), "Vol %u mL", d->volumen_total_ml);
    u8g2_DrawStr(g, 6, 38, texto);
    snprintf(texto, sizeof(texto), (d->duracion_min % 60) ? "Tiempo %u.5 h" : "Tiempo %u h", d->duracion_min / 60);
    u8g2_DrawStr(g, 6, 49, texto);
    Pie(g, "Pulsar: iniciar");
    u8g2_SendBuffer(g);
}

void Pant_AjusteGoteo(u8g2_t *g, uint16_t gotas_min, uint16_t volumen_ml, uint16_t duracion_min, uint16_t factor_goteo) {
    char texto[32];
    uint8_t ancho_numero, x;

    Preparar(g);
    Cabecera(g, "Ajuste de goteo", NULL);

    // El cálculo, con los datos que cargó la enfermera: volumen en tiempo, y el factor del equipo
    snprintf(texto, sizeof(texto), (duracion_min % 60) ? "%u mL en %u.5 h, FG %u" : "%u mL en %u h, FG %u",
             volumen_ml, duracion_min / 60, factor_goteo);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    u8g2_DrawStr(g, Centrar(g, texto), 26, texto);

    // Resultado grande con su unidad, centrado
    snprintf(texto, sizeof(texto), "%u", gotas_min);
    u8g2_SetFont(g, u8g2_font_10x20_tr);
    ancho_numero = u8g2_GetStrWidth(g, texto);
    x = (ANCHO - (ancho_numero + 6 + 42)) / 2;   // 42 px es el ancho de "gtt/min" en la fuente chica
    u8g2_DrawStr(g, x, 47, texto);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    u8g2_DrawStr(g, x + ancho_numero + 6, 47, "gtt/min");

    Pie(g, "Pulsar: seguir");
    u8g2_SendBuffer(g);
}

void Pant_Alarma(u8g2_t *g, bool invertida, const char *nivel, const char *titulo, const char *detalle) {
    Preparar(g);
    if (invertida) {
        u8g2_DrawBox(g, 0, 0, ANCHO, 64);
        u8g2_SetDrawColor(g, 0);
    }
    u8g2_SetFont(g, u8g2_font_6x13_tr);
    u8g2_DrawStr(g, 2, 11, nivel);
    u8g2_DrawHLine(g, 0, Y_LINEA_CAB, ANCHO);

    // El título entra con la fuente grande si es corto; si no, con una más chica
    u8g2_SetFont(g, u8g2_font_10x20_tr);
    if (u8g2_GetStrWidth(g, titulo) > ANCHO_UTIL) {
        u8g2_SetFont(g, u8g2_font_9x15_tr);
    }
    u8g2_DrawStr(g, Centrar(g, titulo), 38, titulo);
    TextoCentrado(g, 49, detalle, u8g2_font_6x10_tr, u8g2_font_5x7_tr);

    Pie(g, "Pulsar: silenciar");
    u8g2_SendBuffer(g);
}

void Pant_Fin(u8g2_t *g, uint8_t bateria_pct, uint16_t volumen_ml, uint32_t tiempo_s) {
    char texto[24];

    Preparar(g);
    snprintf(texto, sizeof(texto), "BAT %u%%", bateria_pct);
    Cabecera(g, texto, NULL);

    u8g2_SetFont(g, u8g2_font_6x13_tr);
    u8g2_DrawStr(g, Centrar(g, "Infusion finalizada"), 32, "Infusion finalizada");
    snprintf(texto, sizeof(texto), "%u mL %lu:%02lu h", volumen_ml, (unsigned long)(tiempo_s / 3600),
             (unsigned long)((tiempo_s / 60) % 60));
    u8g2_DrawStr(g, Centrar(g, texto), 47, texto);
    Pie(g, "Doble click: menu");
    u8g2_SendBuffer(g);
}

void Pant_Apagar(u8g2_t *g, bool si_elegido) {
    const char *textos[2] = {"No", "Si"};
    const uint8_t x[2] = {16, 72}, y = 22, ancho = 40, alto = 22;

    Preparar(g);
    Cabecera(g, "Apagar equipo?", NULL);
    u8g2_SetFont(g, u8g2_font_10x20_tr);
    for (uint8_t i = 0; i < 2; i++) {
        bool elegido = (i == 1) == si_elegido;
        if (elegido) {
            u8g2_DrawBox(g, x[i], y, ancho, alto);
            u8g2_SetDrawColor(g, 0);
        } else {
            u8g2_DrawFrame(g, x[i], y, ancho, alto);
        }
        u8g2_DrawStr(g, x[i] + (ancho - u8g2_GetStrWidth(g, textos[i])) / 2, y + 18, textos[i]);
        u8g2_SetDrawColor(g, 1);
    }
    Pie(g, "Giro: elegir Pulsar: OK");
    u8g2_SendBuffer(g);
}
