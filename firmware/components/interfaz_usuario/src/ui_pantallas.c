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
    char texto[32];
    const ui_chequeo_t *falla = NULL;

    (void)todos_ok;
    for (uint8_t i = 0; i < cantidad && i < UI_CHEQUEOS_MAX && falla == NULL; i++) {
        if (chequeos[i].nombre != NULL && chequeos[i].estado == UI_CHEQUEO_FALLA) {
            falla = &chequeos[i];
        }
    }

    Preparar(g);
    snprintf(texto, sizeof(texto), "BAT %u%%", bateria_pct);
    Cabecera(g, "Autotest", texto);
    if (falla != NULL) {
        snprintf(texto, sizeof(texto), "Falla: %s", falla->nombre);
        TextoCentrado(g, 31, texto, u8g2_font_6x13_tr, u8g2_font_6x10_tr);
        TextoCentrado(g, 46, (falla->accion != NULL) ? falla->accion : "No usar", u8g2_font_6x13_tr, u8g2_font_6x10_tr);
        Pie(g, "Reintentando...");
    } else {
        TextoCentrado(g, 36, "Chequeando...", u8g2_font_6x13_tr, u8g2_font_6x10_tr);
    }
    u8g2_SendBuffer(g);
}

void Pant_Acceso(u8g2_t *g, const uint8_t *digitos, uint8_t posicion, bool error, uint16_t bloqueo_s) {
    char espera[20];
    const uint8_t ancho = 20, alto = 24, separacion = 8, x0 = 12, y0 = 20;
    char caracter[2] = {0, 0};

    Preparar(g);
    Cabecera(g, "Clave", NULL);
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
    if (bloqueo_s > 0) {
        snprintf(espera, sizeof(espera), "Espere %u s", bloqueo_s);   // tres errores seguidos: bloqueo temporal
        Pie(g, espera);
    } else {
        Pie(g, error ? "Clave incorrecta" : "Giro: +/- Pulsar: OK");
    }
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

void Pant_Resumen(u8g2_t *g, const ui_datos_t *d, uint8_t pagina, uint8_t motivo) {
    char texto[32];

    Preparar(g);
    Cabecera(g, motivo == 2 ? "Memoria llena" : "Confirmar", pagina == 0 ? "1/2" : "2/2");   // sin lugar en el registro: no se puede iniciar
    if (pagina == 0) {
        // El factor de goteo mal cargado es el error más peligroso: va grande y con la leyenda de verificarlo
        u8g2_SetFont(g, u8g2_font_10x20_tr);
        snprintf(texto, sizeof(texto), "FG %u", d->factor_goteo);
        uint8_t ancho = u8g2_GetStrWidth(g, texto);
        uint8_t x = (ANCHO - (ancho + 4 + 36)) / 2;   // 36 px es el ancho de "gtt/mL" en la fuente chica
        u8g2_DrawStr(g, x, 36, texto);
        u8g2_SetFont(g, u8g2_font_6x10_tr);
        u8g2_DrawStr(g, x + ancho + 4, 36, "gtt/mL");
        u8g2_SetFont(g, u8g2_font_5x7_tr);
        u8g2_DrawStr(g, Centrar(g, "Verificar en el envase"), 44, "Verificar en el envase");
        u8g2_DrawStr(g, Centrar(g, "de la guia"), 51, "de la guia");
        Pie(g, "Pulsar: seguir");
    } else {
        u8g2_SetFont(g, u8g2_font_6x10_tr);
        snprintf(texto, sizeof(texto), "Volumen  %u mL", d->volumen_total_ml);
        u8g2_DrawStr(g, 6, 26, texto);
        snprintf(texto, sizeof(texto), "Duracion %u:%02u h", d->duracion_min / 60, d->duracion_min % 60);
        u8g2_DrawStr(g, 6, 37, texto);
        // Caudal objetivo en mL/h y en gotas/min, para compararlo con la prescripción
        float gotas_min = (d->duracion_min == 0) ? 0.0f : (float)d->volumen_total_ml * d->factor_goteo / d->duracion_min;
        snprintf(texto, sizeof(texto), "%u mL/h  %.1f gtt/min", d->flujo_objetivo_mlh, (double)gotas_min);
        u8g2_SetFont(g, u8g2_font_5x8_tr);
        u8g2_DrawStr(g, Centrar(g, texto), 48, texto);
        Pie(g, motivo == 0 ? "Pulsar: iniciar" : motivo == 1 ? "Menos de 2 gtt/min: no" : "Llamar a soporte tecnico");
    }
    u8g2_SendBuffer(g);
}

void Pant_AjusteGoteo(u8g2_t *g, uint16_t gotas_min, uint16_t volumen_ml, uint16_t duracion_min, uint16_t factor_goteo, bool recordar) {
    char texto[32];
    uint8_t ancho_numero, x;

    Preparar(g);
    snprintf(texto, sizeof(texto), "FG %u", factor_goteo);   // factor de goteo del equipo, arriba a la derecha
    Cabecera(g, "Ajuste de goteo", texto);

    // El cálculo, con los datos que cargó la enfermera: el volumen en el tiempo indicado
    snprintf(texto, sizeof(texto), "%u mL en %u:%02u h", volumen_ml, duracion_min / 60, duracion_min % 60);
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

    Pie(g, recordar ? "Confirmar ajuste" : "Pulsar: seguir");   // a los 10 min sin confirmar el pie lo recuerda (con un beep por minuto)
    u8g2_SendBuffer(g);
}

void Pant_Alarma(u8g2_t *g, bool invertida, const char *nivel, const char *derecha, const char *titulo, const char *detalle) {
    Preparar(g);
    if (invertida) {
        u8g2_DrawBox(g, 0, 0, ANCHO, 64);
        u8g2_SetDrawColor(g, 0);
    }
    u8g2_SetFont(g, u8g2_font_6x13_tr);
    u8g2_DrawStr(g, 2, 11, nivel);
    if (derecha != NULL && derecha[0] != '\0') {
        u8g2_DrawStr(g, ANCHO - 2 - u8g2_GetStrWidth(g, derecha), 11, derecha);   // dato corto a la derecha de la cabecera (p. ej. el % de desvío)
    }
    u8g2_DrawHLine(g, 0, Y_LINEA_CAB, ANCHO);

    // Todas las alarmas usan la misma letra: el título más largo ("BATERIA BAJA") mide 120 px y
    // el detalle más largo entra en 124 px, así que no hace falta achicar ninguna
    u8g2_SetFont(g, u8g2_font_10x20_tr);
    u8g2_DrawStr(g, Centrar(g, titulo), 38, titulo);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    u8g2_DrawStr(g, Centrar(g, detalle), 49, detalle);

    Pie(g, "Pulsar: silenciar");
    u8g2_SetDrawColor(g, 1);   // con fondo blanco se dibujó en color 0: se restablece para que la próxima pantalla no quede negra sobre negro
    u8g2_SendBuffer(g);
}

void Pant_SinInfusion(u8g2_t *g, uint8_t bateria_pct) {
    char texto[24];

    Preparar(g);
    snprintf(texto, sizeof(texto), "BAT %u%%", bateria_pct);
    Cabecera(g, texto, NULL);

    u8g2_SetFont(g, u8g2_font_6x13_tr);
    u8g2_DrawStr(g, Centrar(g, "Sin infusion activa"), 36, "Sin infusion activa");
    Pie(g, "Mantener 3s: ajustes");
    u8g2_SendBuffer(g);
}

void Pant_Mensaje(u8g2_t *g, const char *texto) {
    Preparar(g);
    TextoCentrado(g, 36, texto, u8g2_font_6x13_tr, u8g2_font_6x10_tr);
    u8g2_SendBuffer(g);
}

void Pant_Diagnostico(u8g2_t *g, const ui_datos_t *d) {
    char texto[32];

    Preparar(g);
    Cabecera(g, "Diagnostico", NULL);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    snprintf(texto, sizeof(texto), "Base: %u / %u mV", d->base_optica_mv[0], d->base_optica_mv[1]);
    u8g2_DrawStr(g, 4, 26, texto);
    snprintf(texto, sizeof(texto), "Angulo: %.1f grados", (double)d->angulo_grados);
    u8g2_DrawStr(g, 4, 36, texto);
    snprintf(texto, sizeof(texto), "Bat: %u mV  %u%%", d->bateria_mv, d->bateria_pct);
    u8g2_DrawStr(g, 4, 46, texto);
    snprintf(texto, sizeof(texto), "FW %s  Reg %u%s", (d->version != NULL) ? d->version : "?", d->registro_lineas,
             d->registro_descargado ? "" : " *");   // * = registro sin descargar
    Pie(g, texto);
    u8g2_SendBuffer(g);
}

void Pant_Confirmar(u8g2_t *g, const char *titulo, const char *linea1, const char *linea2, bool si_elegido) {
    const char *textos[2] = {"No", "Si"};
    const uint8_t x[2] = {20, 72}, y = 39, ancho = 36, alto = 12;

    Preparar(g);
    Cabecera(g, titulo, NULL);
    TextoCentrado(g, 26, linea1, u8g2_font_6x10_tr, u8g2_font_5x7_tr);
    TextoCentrado(g, 36, linea2, u8g2_font_6x10_tr, u8g2_font_5x7_tr);
    u8g2_SetFont(g, u8g2_font_6x10_tr);
    for (uint8_t i = 0; i < 2; i++) {
        bool elegido = (i == 1) == si_elegido;
        if (elegido) {
            u8g2_DrawBox(g, x[i], y, ancho, alto);
            u8g2_SetDrawColor(g, 0);
        } else {
            u8g2_DrawFrame(g, x[i], y, ancho, alto);
        }
        u8g2_DrawStr(g, x[i] + (ancho - u8g2_GetStrWidth(g, textos[i])) / 2, y + 10, textos[i]);
        u8g2_SetDrawColor(g, 1);
    }
    Pie(g, "Giro: elegir Pulsar: OK");
    u8g2_SendBuffer(g);
}
