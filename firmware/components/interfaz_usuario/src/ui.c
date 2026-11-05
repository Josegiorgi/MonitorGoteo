/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "ui.h"
#include "ui_pantallas.h"
#include "i2c_mcu.h"
#include "display.h"
#include "refresh_wait_32_32_28f.h"

/*==================[macros and definitions]=================================*/
#define SSD1315_ANCHO  128
#define I2C_FREQ_HZ    100000   /* 100 kHz: el mismo bus lo comparte el ADXL345 */

#define UI_ALARMA_PARPADEA   0    /*!< 1 hace que la pantalla de alarma alterne entre fondo blanco y negro */
#define CODIGO_DIGITOS       4
#define CLAVE_ERRORES_MAX    3        /*!< Errores seguidos de clave que activan el bloqueo */
#define CLAVE_BLOQUEO_S_DEF  60       /*!< Segundos de bloqueo después de CLAVE_ERRORES_MAX errores */
#define TECNICO_INACTIVIDAD_S_DEF 300     /*!< Segundos sin actividad en el Modo Técnico para salir solo (con un reinicio) */
#define TECNICO_ITEMS        4        /*!< Modo Técnico: Diagnóstico, Probar alarma, Borrar registro y Salir */
#define PRUEBA_ITEMS         9        /*!< Alarmas que se pueden probar (una por patrón del buzzer) */
#define INACTIVIDAD_S_DEF    60       /*!< Segundos sin tocar nada en la clave y en el menú para volver al Monitoreo */
#define CONFIG_PARAMETROS    3
#define MENU_FILAS_VISIBLES  3
#define MENU_ITEMS           4   /*!< Ajustes: Cambiar datos, Detener monitoreo, Apagar y Volver */
#define SIN_INFUSION_ITEMS   2   /*!< Sin infusión (sesión abierta): Nueva infusión y Apagar */

/*==================[internal data definition]===============================*/
/* El u8g2_t vive en el componente display (la OLED va por i2c_mcu, no por el HAL de u8g2) */
#define u8g2 (*Display_Handle())

/* Ícono de volumen, 8x6 (generado con Lopaka) */
static const unsigned char icono_volumen[] U8X8_PROGMEM = {0x48, 0x8c, 0xaf, 0xaf, 0x8c, 0x48};

/* Tilde de la pantalla de colocación, 10x9 (generado con Lopaka) */
static const unsigned char icono_tilde[] U8X8_PROGMEM = {0x00, 0x03, 0x80, 0x03, 0x80, 0x01, 0xc0, 0x00, 0xe3, 0x00, 0x77, 0x00, 0x3e, 0x00, 0x1c, 0x00, 0x08, 0x00};

/* Ícono de error de la pantalla de colocación, 18x18 (generado con Lopaka) */
static const unsigned char icono_error[] U8X8_PROGMEM = {0xc0,0x0f,0x00,0xf0,0x3f,0x00,0xf8,0x7f,0x00,0xfc,0xff,0x00,0xce,0xcf,0x01,0x8e,0xc7,0x01,0x1f,0xe3,0x03,0x3f,0xf0,0x03,0x7f,0xf8,0x03,0x7f,0xf8,0x03,0x3f,0xf0,0x03,0x1f,0xe3,0x03,0x8e,0xc7,0x01,0xce,0xcf,0x01,0xfc,0xff,0x00,0xf8,0x7f,0x00,0xf0,0x3f,0x00,0xc0,0x0f,0x00};

/* Puntos suspensivos que siguen a "Iniciando", 5x7 (generado con Lopaka) */
static const unsigned char icono_puntos[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15};

/* Parámetros de la pantalla Config, en el orden en que se muestran */
typedef struct {
    const char *nombre;
    const char *unidad;
    const uint16_t *lista;           // si no es NULL, el parámetro toma solo los valores de esta lista
    uint8_t cantidad;
    uint16_t minimo, maximo, paso;   // si no, va de minimo a maximo de a paso
} parametro_t;

static const uint16_t factores_goteo[] = {10, 15, 20, 60};          /* gtt/mL: 20 macrogoteo, 60 microgoteo */
static const uint16_t volumenes_ml[] = {50, 100, 250, 500, 1000};   /* sachets habituales */

static const parametro_t parametros[CONFIG_PARAMETROS] = {
    {"Factor de goteo",  "gtt/mL", factores_goteo, sizeof(factores_goteo) / sizeof(factores_goteo[0]), 0, 0, 0},
    {"Volumen total",    "mL",     volumenes_ml,   sizeof(volumenes_ml) / sizeof(volumenes_ml[0]),     0, 0, 0},
    {"Tiempo total",     "h:min",  NULL,           0,                                                  10, 1440, 10},   // en minutos, de 10 min a 24 h
};

/* Estado de la navegación */
static ui_pantalla_t pantalla = UI_PANTALLA_AUTOTEST;
static const ui_chequeo_t *chequeos_autotest = NULL;
static uint8_t chequeos_cantidad = 0;
static uint8_t acceso_digitos[CODIGO_DIGITOS];
static uint8_t acceso_posicion = 0;
static bool acceso_error = false;
static bool acceso_arranque = false;     // true: es la clave del arranque (no se cancela ni vence)
static uint8_t acceso_errores = 0;       // errores de clave seguidos
static int64_t bloqueo_hasta_us = 0;     // hasta cuándo está bloqueada la clave (0 = no lo está)
static int64_t ultima_actividad_us = 0;  // última vez que se tocó el encoder en la clave o el menú
static uint8_t config_indice = 0;
static uint8_t resumen_pagina = 0;       // 0: factor de goteo destacado, 1: volumen, duración y caudal objetivo
static uint8_t menu_cursor = 0;
static uint8_t menu_primero = 0;
static bool apagar_si = false;           // respuesta elegida en las confirmaciones (Detener y Apagar)
static ui_pantalla_t apagar_origen = UI_PANTALLA_AJUSTES;   // a dónde se vuelve si se contesta "No" a Apagar
static bool sin_infusion_abierta = false; // true: sesión abierta en "Sin infusión activa" (Nueva infusión / Apagar)
static bool ajuste_es_cambio = false;     // true si el Ajuste de goteo viene de Cambiar datos (no se reinicia el contador)
static uint16_t duracion_previa = 0;      // duración antes de editarla desde Ajustes (se restituye si se cancela)
static ui_pantalla_t acceso_origen = UI_PANTALLA_MONITOREO;   // pantalla a la que se vuelve si se cancela el acceso
static bool config_desde_menu = false;   // true si Config se abrió desde Ajustes > Cambiar datos (y no en una infusión nueva)
static bool apagado_pedido = false;
static bool parpadeo = false;
static ui_alarma_t ultima_alarma = UI_ALARMA_NINGUNA;
static ui_pantalla_t alarma_origen = UI_PANTALLA_MONITOREO;   // pantalla a la que se vuelve al silenciar o resolverse la alarma
static ui_clave_cb_t clave_cb = NULL;
static uint32_t t_clave_bloqueo_s = CLAVE_BLOQUEO_S_DEF;           // se pueden acortar para probar (UI_SetTiempos)
static uint32_t t_inactividad_s = INACTIVIDAD_S_DEF;
static uint32_t t_tecnico_inactividad_s = TECNICO_INACTIVIDAD_S_DEF;
static uint8_t tecnico_cursor = 0, tecnico_primero = 0;   // Modo Técnico: menú principal
static uint8_t prueba_cursor = 0, prueba_primero = 0;     // Modo Técnico: lista de alarmas para probar
static bool salir_pedido = false;                          // se eligió Salir del Modo Técnico o venció la inactividad

/*==================[internal functions definition]==========================*/
static void PantallaColocacion(const ui_datos_t *d) {
    char texto[32];

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);   // no depender del color que haya dejado la pantalla anterior (la de alarma dibuja en 0)
    u8g2_SetFontMode(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);
    if (d->colocacion_ok) {
        u8g2_DrawStr(&u8g2, 7, 19, "Colocacion correcta");
        u8g2_DrawXBMP(&u8g2, 51, 32, 10, 9, icono_tilde);
        u8g2_DrawStr(&u8g2, 62, 41, "OK");
    } else {
        u8g2_DrawStr(&u8g2, 1, 19, "Colocacion incorrecta");
        u8g2_DrawXBMP(&u8g2, 56, 28, 18, 18, icono_error);
    }

    // Rango correcto, para que se entienda qué ángulo se espera (provisorio)
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    snprintf(texto, sizeof(texto), "Rango: +/- %.1f grados", (double)d->tolerancia_grados);
    u8g2_DrawStr(&u8g2, (SSD1315_ANCHO - u8g2_GetStrWidth(&u8g2, texto)) / 2, 27, texto);

    // Ángulo medido, centrado, con el circulito de grados (la fuente no tiene el símbolo, se dibuja)
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    snprintf(texto, sizeof(texto), "%.1f", (double)d->angulo_grados);
    uint8_t ancho = u8g2_GetStrWidth(&u8g2, texto);
    uint8_t x = (SSD1315_ANCHO - (ancho + 5)) / 2;
    u8g2_DrawStr(&u8g2, x, 59, texto);
    u8g2_DrawCircle(&u8g2, x + ancho + 3, 52, 1, U8G2_DRAW_ALL);

    u8g2_SendBuffer(&u8g2);
}

/* Texto del aviso que queda abajo en el Monitoreo cuando hay una alarma silenciada, un aviso de batería o pausa */
static const char *TextoAviso(const ui_datos_t *d) {
    if (d->alarma_silenciada) {
        switch (d->alarma) {
        case UI_ALARMA_OCLUSION:         return "! OCLUSION";
        case UI_ALARMA_DESVIACION_FLUJO: return "! DESVIACION FLUJO";
        case UI_ALARMA_FIN_SACHET:       return "! FIN SACHET";
        case UI_ALARMA_BATERIA_BAJA:     return "! BATERIA BAJA";
        case UI_ALARMA_COLOCACION:       return "! COLOCACION";
        case UI_ALARMA_FALLA_EQUIPO:     return "! FALLA EQUIPO";
        case UI_ALARMA_GOTEO_SIN_MONITOREO: return "! GOTEO SIN MONIT.";
        default: break;
        }
    }
    if (d->aviso_bateria) {
        return "! AVISO BATERIA 20%";   // la batería llegó al 20 %: sin sonido, solo este aviso
    }
    if (d->monitoreo_reanudado) {
        return "Monitoreo reanudado";   // se retomó tras un reinicio
    }
    return d->en_pausa ? "|| PAUSA" : NULL;
}

/* gotas/min objetivo con decimales (volumen x gtt/mL / minutos): 500 mL en 8 h con 20 gtt/mL son 20,8 */
static float GotasMinObjetivoF(const ui_datos_t *d) {
    return (d->duracion_min == 0) ? 0.0f : (float)d->volumen_total_ml * d->factor_goteo / d->duracion_min;
}

/* Por qué no se puede iniciar desde el Resumen: 0 = se puede, 1 = menos de 2 gotas/min, 2 = no queda lugar en la memoria del registro */
static uint8_t MotivoResumen(const ui_datos_t *d) {
    if (GotasMinObjetivoF(d) < 2.0f) {
        return 1;
    }
    return (d->duracion_min > d->registro_minutos_libres) ? 2 : 0;
}

static void PantallaMonitoreo(const ui_datos_t *d) {
    char texto[16];
    const char *aviso = TextoAviso(d);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);   // no depender del color que haya dejado la pantalla anterior (la de alarma dibuja en 0)
    u8g2_SetFontMode(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    // Barra superior: batería, factor de goteo, ícono de volumen (no está si se silenció la alarma)
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    snprintf(texto, sizeof(texto), "BAT: %u%%", d->bateria_pct);
    u8g2_DrawStr(&u8g2, 3, 12, texto);
    snprintf(texto, sizeof(texto), "FG%u", d->factor_goteo);
    u8g2_DrawStr(&u8g2, 87, 12, texto);
    if (!d->alarma_silenciada) {
        u8g2_DrawXBMP(&u8g2, 115, 5, 8, 6, icono_volumen);
    }
    u8g2_DrawLine(&u8g2, 1, 16, 127, 16);

    // Centro: caudal grande, y volumen y tiempo al costado
    u8g2_SetFont(&u8g2, u8g2_font_profont29_tr);
    snprintf(texto, sizeof(texto), "%u", d->flujo_mlh);
    u8g2_DrawStr(&u8g2, 6, 43, texto);

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 54, 44, "mL/h");

    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    snprintf(texto, sizeof(texto), d->volumen_incierto ? ">%uml" : "%uml", d->volumen_ml);   // ">" = como mínimo (se reinició sin conocer el tiempo apagado)
    u8g2_DrawStr(&u8g2, d->volumen_incierto ? 86 : 92, 30, texto);
    snprintf(texto, sizeof(texto), "%02lu:%02lu", (unsigned long)(d->tiempo_s / 3600),
             (unsigned long)((d->tiempo_s / 60) % 60));
    u8g2_DrawStr(&u8g2, 91, 46, texto);

    // Pie: aviso de alarma silenciada o pausa (sobre fondo blanco) o la pista del menú
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    if (aviso != NULL) {
        u8g2_DrawBox(&u8g2, 0, 51, SSD1315_ANCHO, 13);
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawStr(&u8g2, (SSD1315_ANCHO - u8g2_GetStrWidth(&u8g2, aviso)) / 2, 61, aviso);
        u8g2_SetDrawColor(&u8g2, 1);
    } else {
        u8g2_DrawLine(&u8g2, 1, 50, 127, 50);
        u8g2_DrawStr(&u8g2, (SSD1315_ANCHO - u8g2_GetStrWidth(&u8g2, "Mantener 3s: ajustes")) / 2, 61, "Mantener 3s: ajustes");
    }

    u8g2_SendBuffer(&u8g2);
}

static void PantallaAlarma(const ui_datos_t *d) {
    char detalle[32];
    char derecha[12] = "";
    const char *nivel = "ALARMA ALTA", *titulo = "";

    switch (d->alarma) {
    case UI_ALARMA_OCLUSION:
        titulo = "OCLUSION";
        snprintf(detalle, sizeof(detalle), "Sin gotas hace %u s", d->segundos_sin_gotas);
        break;
    case UI_ALARMA_DESVIACION_FLUJO:
        nivel = "ALARMA MEDIA";
        titulo = (d->desvio_pct > 0) ? "FLUJO ALTO" : "FLUJO BAJO";
        // Porcentaje de desvío arriba a la derecha y, abajo, las gotas por minuto a las que hay que volver
        snprintf(derecha, sizeof(derecha), "%+d%%", d->desvio_pct);
        snprintf(detalle, sizeof(detalle), "Obj: %.1f gtt/min", (double)GotasMinObjetivoF(d));
        break;
    case UI_ALARMA_FIN_SACHET:
        titulo = "FIN SACHET";
        snprintf(detalle, sizeof(detalle), "%u mL completados", d->volumen_total_ml);
        break;
    case UI_ALARMA_FALLA_EQUIPO:
        titulo = "FALLA EQUIPO";
        snprintf(detalle, sizeof(detalle), "Llamar a enfermeria");
        break;
    case UI_ALARMA_GOTEO_SIN_MONITOREO:
        nivel = "ALARMA MEDIA";
        titulo = "SIN MONITOREO";
        snprintf(detalle, sizeof(detalle), "Hay goteo: llamar a enf.");
        break;
    case UI_ALARMA_BATERIA_BAJA:
        nivel = "ALARMA MEDIA";
        titulo = "BATERIA BAJA";
        snprintf(detalle, sizeof(detalle), "Carga: %u%%", d->bateria_pct);
        break;
    default:
        detalle[0] = '\0';
        break;
    }
    // Las alarmas van con fondo blanco; con UI_ALARMA_PARPADEA alternan con fondo negro
    Pant_Alarma(&u8g2, UI_ALARMA_PARPADEA ? parpadeo : true, nivel, derecha, titulo, detalle);
}

static bool TodosOk(void) {
    for (uint8_t i = 0; i < chequeos_cantidad; i++) {
        if (chequeos_autotest[i].estado != UI_CHEQUEO_OK) {
            return false;
        }
    }
    return true;
}

/* Valor actual del parámetro i de la pantalla Config */
static uint16_t *Campo(ui_datos_t *d, uint8_t i) {
    switch (i) {
    case 0:  return &d->factor_goteo;
    case 1:  return &d->volumen_total_ml;
    default: return &d->duracion_min;
    }
}

/* El caudal objetivo no lo carga nadie: sale de volumen / tiempo (mL/h), redondeado */
static void ActualizarCaudalObjetivo(ui_datos_t *d) {
    if (d->duracion_min > 0) {
        d->flujo_objetivo_mlh = ((uint32_t)d->volumen_total_ml * 60 + d->duracion_min / 2) / d->duracion_min;
    }
}

/* Valor del parámetro i como texto para la pantalla Config (el tiempo se muestra en horas) */
static void ValorTexto(ui_datos_t *d, uint8_t i, char *texto, size_t tam) {
    uint16_t valor = *Campo(d, i);
    if (i == 2) {
        snprintf(texto, tam, "%u:%02u", valor / 60, valor % 60);   // horas:minutos
    } else {
        snprintf(texto, tam, "%u", valor);
    }
}

/* Sube (+1) o baja (-1) el parámetro i un paso */
static void CambiarParametro(ui_datos_t *d, uint8_t i, int8_t sentido) {
    uint16_t *valor = Campo(d, i);

    if (parametros[i].lista != NULL) {
        const uint8_t n = parametros[i].cantidad;
        uint8_t k = 0;
        while (k < n - 1 && parametros[i].lista[k] != *valor) {
            k++;   // si el valor actual no está en la lista, parte del último
        }
        k = (sentido > 0) ? (k + 1) % n : (k + n - 1) % n;
        *valor = parametros[i].lista[k];
    } else {
        int32_t nuevo = (int32_t)*valor + sentido * parametros[i].paso;
        if (nuevo < parametros[i].minimo) {
            nuevo = parametros[i].minimo;
        } else if (nuevo > parametros[i].maximo) {
            nuevo = parametros[i].maximo;
        }
        *valor = nuevo;
    }
    ActualizarCaudalObjetivo(d);
}

/* Segundos que faltan para que termine el bloqueo de la clave (0 si no está bloqueada) */
static uint16_t BloqueoRestanteS(void) {
    int64_t resta_us = bloqueo_hasta_us - esp_timer_get_time();
    if (bloqueo_hasta_us == 0 || resta_us <= 0) {
        return 0;
    }
    return (uint16_t)((resta_us + 999999) / 1000000);
}

static void IrAcceso(void) {
    if (pantalla != UI_PANTALLA_ACCESO) {
        acceso_origen = pantalla;   // desde el Monitoreo o desde Fin de infusión
    }
    memset(acceso_digitos, 0, sizeof(acceso_digitos));
    acceso_posicion = 0;
    acceso_error = false;
    ultima_actividad_us = esp_timer_get_time();
    pantalla = UI_PANTALLA_ACCESO;
}

/* true si lo ingresado coincide con el código (la clave clínica o el código técnico) */
static bool CodigoCoincide(const char *codigo) {
    bool coincide = true;
    for (uint8_t i = 0; i < CODIGO_DIGITOS; i++) {
        coincide = coincide && (acceso_digitos[i] == codigo[i] - '0');
    }
    return coincide;
}

static bool EnModoTecnico(void) {
    return pantalla == UI_PANTALLA_TECNICO || pantalla == UI_PANTALLA_DIAGNOSTICO ||
           pantalla == UI_PANTALLA_PROBAR_ALARMA || pantalla == UI_PANTALLA_BORRAR_REGISTRO;
}

static void IrTecnico(void) {
    ultima_actividad_us = esp_timer_get_time();
    tecnico_cursor = 0;
    tecnico_primero = 0;
    pantalla = UI_PANTALLA_TECNICO;
}

/* Mueve el cursor de una lista de "total" ítems con 3 filas visibles y ajusta cuál es la primera visible */
static void MoverCursor(uint8_t *cursor, uint8_t *primero, uint8_t total, int8_t sentido) {
    *cursor = (*cursor + total + sentido) % total;
    if (*cursor < *primero) {
        *primero = *cursor;
    } else if (*cursor >= *primero + MENU_FILAS_VISIBLES) {
        *primero = *cursor - MENU_FILAS_VISIBLES + 1;
    }
}

static void IrAjustes(void) {
    ultima_actividad_us = esp_timer_get_time();
    menu_cursor = 0;
    menu_primero = 0;
    pantalla = UI_PANTALLA_AJUSTES;
}

/* Sin infusión activa con la sesión abierta (después de poner la clave o de detener el monitoreo) */
static void AbrirSinInfusion(void) {
    ultima_actividad_us = esp_timer_get_time();
    sin_infusion_abierta = true;
    menu_cursor = 0;
    menu_primero = 0;
    pantalla = UI_PANTALLA_FIN;
}

/* Desde Ajustes solo se edita la duración (último parámetro): el factor de goteo y el volumen no se
 * cambian a mitad de la infusión porque invalidarían el volumen ya contado */
static void IrConfig(bool desde_menu) {
    config_desde_menu = desde_menu;
    config_indice = desde_menu ? CONFIG_PARAMETROS - 1 : 0;
    pantalla = UI_PANTALLA_CONFIG;
}

/* Termina la infusión (Detener monitoreo o Apagar con una infusión activa) */
static void FinalizarInfusion(ui_datos_t *d) {
    d->infusion_configurada = false;
    d->alarma = UI_ALARMA_NINGUNA;
    d->alarma_silenciada = false;
    d->en_pausa = false;
}

/* gotas/min para pasar el volumen en el tiempo indicado: volumen x gtt/mL / minutos, redondeado
 * (por ejemplo 500 mL en 8 h con 20 gtt/mL: 500 x 20 / 480 = 21 gtt/min) */
static uint16_t GotasMinObjetivo(const ui_datos_t *d) {
    if (d->duracion_min == 0) {
        return 0;
    }
    return ((uint32_t)d->volumen_total_ml * d->factor_goteo + d->duracion_min / 2) / d->duracion_min;
}

static void DibujarPantalla(const ui_datos_t *d) {
    char valor[12];
    const char *items[MENU_ITEMS] = {"Cambiar datos", "Detener monitoreo", "Apagar", "Volver"};
    const char *items_sin_infusion[SIN_INFUSION_ITEMS] = {"Nueva infusion", "Apagar"};
    const char *items_tecnico[TECNICO_ITEMS] = {"Diagnostico", "Probar alarma", "Borrar registro", "Salir"};
    const char *items_prueba[PRUEBA_ITEMS] = {"Volumen completo", "Flujo detenido", "Fin de sachet", "Colocacion", "Falla equipo",
                                              "Bateria critica", "Desvio fuerte", "Goteo s/monitoreo", "Bateria baja"};

    switch (pantalla) {
    case UI_PANTALLA_AUTOTEST:
        Pant_Autotest(&u8g2, d->bateria_pct, chequeos_autotest, chequeos_cantidad, TodosOk());
        break;
    case UI_PANTALLA_COLOCACION:
        PantallaColocacion(d);
        break;
    case UI_PANTALLA_ACCESO:
        Pant_Acceso(&u8g2, acceso_digitos, acceso_posicion, acceso_error, BloqueoRestanteS());
        break;
    case UI_PANTALLA_CONFIG:
        ValorTexto((ui_datos_t *)d, config_indice, valor, sizeof(valor));
        Pant_Config(&u8g2, config_indice + 1, CONFIG_PARAMETROS, parametros[config_indice].nombre, valor, parametros[config_indice].unidad);
        break;
    case UI_PANTALLA_RESUMEN:
        Pant_Resumen(&u8g2, d, resumen_pagina, MotivoResumen(d));
        break;
    case UI_PANTALLA_AJUSTE_GOTEO:
        Pant_AjusteGoteo(&u8g2, GotasMinObjetivo(d), d->volumen_total_ml, d->duracion_min, d->factor_goteo, d->recordatorio_ajuste);
        break;
    case UI_PANTALLA_MONITOREO:
        PantallaMonitoreo(d);
        break;
    case UI_PANTALLA_ALARMA:
        PantallaAlarma(d);
        break;
    case UI_PANTALLA_AJUSTES:
        Pant_Menu(&u8g2, "Ajustes", items, MENU_ITEMS, menu_cursor, menu_primero, "Pulsar: elegir");
        break;
    case UI_PANTALLA_DETENER:
        Pant_Confirmar(&u8g2, "Detener monitoreo", "Finalizar infusion?", "Verificar clamp cerrado", apagar_si);
        break;
    case UI_PANTALLA_FIN:
        if (sin_infusion_abierta) {
            Pant_Menu(&u8g2, "Sin infusion activa", items_sin_infusion, SIN_INFUSION_ITEMS, menu_cursor, 0, "Pulsar: elegir");
        } else {
            Pant_SinInfusion(&u8g2, d->bateria_pct);
        }
        break;
    case UI_PANTALLA_TECNICO:
        Pant_Menu(&u8g2, "Modo tecnico", items_tecnico, TECNICO_ITEMS, tecnico_cursor, tecnico_primero, "Pulsar: elegir");
        break;
    case UI_PANTALLA_DIAGNOSTICO:
        Pant_Diagnostico(&u8g2, d);
        break;
    case UI_PANTALLA_PROBAR_ALARMA:
        Pant_Menu(&u8g2, "Probar alarma", items_prueba, PRUEBA_ITEMS, prueba_cursor, prueba_primero,
                  d->probar_alarma ? "Pulsar: parar" : "Pulsar: probar");
        break;
    case UI_PANTALLA_BORRAR_REGISTRO:
        Pant_Confirmar(&u8g2, "Borrar registro", d->registro_descargado ? "Se borra todo el registro" : "El registro no fue descargado",
                       d->registro_descargado ? "Seguro?" : "Borrar igual?", apagar_si);
        break;
    case UI_PANTALLA_APAGAR:
        Pant_Confirmar(&u8g2, "Apagar equipo?", d->infusion_configurada ? "Se finalizara la infusion" : "Se apagara el equipo",
                       "Verificar clamp cerrado", apagar_si);
        break;
    }
}

/*==================[external functions definition]==========================*/
bool UI_Init(void) {
    if (!I2C_initialize(I2C_FREQ_HZ)) {   // si el bus ya estaba iniciado no hace nada
        return false;
    }
    return Display_Init();
}

void UI_PantallaInicio(uint8_t cuadro) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFontMode(&u8g2, 1);
    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_SetFont(&u8g2, u8g2_font_profont15_tr);
    u8g2_DrawStr(&u8g2, 20, 15, "MonitorGoteo");
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(&u8g2, 34, 26, "Iniciando");

    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_DrawXBMP(&u8g2, 79, 19, 5, 7, icono_puntos);
    u8g2_DrawXBMP(&u8g2, 46, 31, 32, 32, refresh_wait_32_32_28f_frames[cuadro % 28]);

    u8g2_SendBuffer(&u8g2);
}

void UI_SetChequeos(const ui_chequeo_t *chequeos, uint8_t cantidad) {
    chequeos_autotest = chequeos;
    chequeos_cantidad = cantidad;
}

ui_pantalla_t UI_PantallaActual(void) {
    return pantalla;
}

bool UI_ApagarPedido(void) {
    return apagado_pedido;
}

void UI_PantallaApagada(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
    u8g2_SetPowerSave(&u8g2, 1);
}

void UI_Ir(ui_pantalla_t destino, ui_datos_t *d) {
    pantalla = destino;
    DibujarPantalla(d);
}

/* Cancela la edición de la duración (Ajustes > Cambiar datos): vuelve al valor que tenía */
static void CancelarEdicion(ui_datos_t *d) {
    d->duracion_min = duracion_previa;
    ActualizarCaudalObjetivo(d);
}

bool UI_SalirPedido(void) {
    return salir_pedido;
}

void UI_PantallaMensaje(const char *texto) {
    Pant_Mensaje(&u8g2, texto);
}

void UI_SetTiempos(uint16_t inactividad_s, uint16_t tecnico_inactividad_s, uint16_t clave_bloqueo_s) {
    t_inactividad_s = (inactividad_s > 0) ? inactividad_s : INACTIVIDAD_S_DEF;
    t_tecnico_inactividad_s = (tecnico_inactividad_s > 0) ? tecnico_inactividad_s : TECNICO_INACTIVIDAD_S_DEF;
    t_clave_bloqueo_s = (clave_bloqueo_s > 0) ? clave_bloqueo_s : CLAVE_BLOQUEO_S_DEF;
}

void UI_SetCallbackClave(ui_clave_cb_t cb) {
    clave_cb = cb;
}

bool UI_EditandoDatos(void) {
    return config_desde_menu && (pantalla == UI_PANTALLA_CONFIG || pantalla == UI_PANTALLA_RESUMEN);
}

void UI_Actualizar(ui_datos_t *d) {
    parpadeo = !parpadeo;
    bool editaba = UI_EditandoDatos();

    // Una alarma distinta de la anterior vuelve a sonar aunque se hubiera silenciado la anterior
    if (d->alarma != ultima_alarma) {
        d->alarma_silenciada = false;
        ultima_alarma = d->alarma;
    }

    if (d->alarma == UI_ALARMA_NINGUNA) {
        d->alarma_silenciada = false;
        // Durante la infusión, la pantalla de Colocación solo aparece por la alarma: al resolverse sola
        // (el ángulo volvió al rango) se vuelve al Monitoreo sin pedir un click
        if (pantalla == UI_PANTALLA_ALARMA) {
            pantalla = alarma_origen;   // Monitoreo, o la pantalla de Sin infusión / clave de la que venía
        } else if (pantalla == UI_PANTALLA_COLOCACION && d->infusion_configurada) {
            pantalla = UI_PANTALLA_MONITOREO;
        }
    } else if (!d->infusion_configurada) {
        // Sin infusión activa: las alarmas (goteo sin monitoreo, batería) se ven en la pantalla Sin infusión y en la clave del arranque
        if (!d->alarma_silenciada && (pantalla == UI_PANTALLA_FIN || pantalla == UI_PANTALLA_ACCESO)) {
            alarma_origen = pantalla;
            pantalla = UI_PANTALLA_ALARMA;
        }
    } else if (d->infusion_configurada) {
        // Una alarma nueva interrumpe también la clave y el menú: para volver hay que ingresar la clave otra vez
        bool en_infusion = (pantalla == UI_PANTALLA_AJUSTE_GOTEO) || (pantalla == UI_PANTALLA_MONITOREO) ||
                           (pantalla == UI_PANTALLA_ACCESO) || (pantalla == UI_PANTALLA_AJUSTES) || (pantalla == UI_PANTALLA_ALARMA) ||
                           (pantalla == UI_PANTALLA_DETENER) || (pantalla == UI_PANTALLA_APAGAR) || (pantalla == UI_PANTALLA_COLOCACION) ||
                           UI_EditandoDatos();
        if (d->alarma == UI_ALARMA_COLOCACION) {
            if (en_infusion && !d->alarma_silenciada) {
                pantalla = UI_PANTALLA_COLOCACION;   // un choque movió el equipo: se vuelve a verificar
            } else if (pantalla == UI_PANTALLA_COLOCACION) {
                pantalla = UI_PANTALLA_MONITOREO;    // silenciada: queda el aviso abajo en el Monitoreo
            }
        } else if (!d->alarma_silenciada && en_infusion) {
            if (pantalla != UI_PANTALLA_ALARMA) {
                alarma_origen = UI_PANTALLA_MONITOREO;
            }
            pantalla = UI_PANTALLA_ALARMA;
        }
    }
    // La clave (salvo la del arranque) y el menú vuelven solos adonde estaban si nadie los usa
    bool inactivo = (esp_timer_get_time() - ultima_actividad_us) > (int64_t)t_inactividad_s * 1000000;
    if (inactivo && !acceso_arranque && (pantalla == UI_PANTALLA_ACCESO || pantalla == UI_PANTALLA_AJUSTES ||
                                         pantalla == UI_PANTALLA_DETENER || pantalla == UI_PANTALLA_APAGAR)) {
        pantalla = acceso_origen;
        if (pantalla == UI_PANTALLA_FIN) {
            sin_infusion_abierta = false;   // sin infusión: se vuelve a la pantalla bloqueada
        }
    }
    if (inactivo && pantalla == UI_PANTALLA_FIN && sin_infusion_abierta) {
        sin_infusion_abierta = false;       // 60 s sin actividad: queda bloqueada en "Sin infusión activa"
    }
    if (EnModoTecnico() && (esp_timer_get_time() - ultima_actividad_us) > (int64_t)t_tecnico_inactividad_s * 1000000) {
        salir_pedido = true;                // 5 min sin actividad: el main reinicia el equipo
    }
    if (editaba && pantalla != UI_PANTALLA_CONFIG && pantalla != UI_PANTALLA_RESUMEN && pantalla != UI_PANTALLA_AJUSTE_GOTEO) {
        CancelarEdicion(d);                 // una alarma interrumpió el cambio de datos: no se aplica
    }
    DibujarPantalla(d);
}

void UI_IniciarClave(ui_datos_t *d) {
    acceso_arranque = true;
    acceso_errores = 0;
    IrAcceso();
    DibujarPantalla(d);
}

void UI_Evento(encoder_evento_t ev, ui_datos_t *d) {
    int8_t sentido = (ev == ENC_GIRO_HORARIO) ? 1 : (ev == ENC_GIRO_ANTIHORARIO) ? -1 : 0;
    bool pulsar = (ev == ENC_CLICK), mantener = (ev == ENC_CLICK_LARGO), doble = (ev == ENC_DOBLE_CLICK);
    bool muy_largo = (ev == ENC_CLICK_MUY_LARGO);   // 3 s apretado: abre la clave

    if (pantalla == UI_PANTALLA_ACCESO || pantalla == UI_PANTALLA_AJUSTES || pantalla == UI_PANTALLA_DETENER ||
        pantalla == UI_PANTALLA_APAGAR || (pantalla == UI_PANTALLA_FIN && sin_infusion_abierta) || EnModoTecnico()) {
        ultima_actividad_us = esp_timer_get_time();
    }

    switch (pantalla) {
    case UI_PANTALLA_AUTOTEST:
        break;   // solo se muestra si un chequeo falla: no deja continuar

    case UI_PANTALLA_COLOCACION:
        if (pulsar && d->infusion_configurada && d->alarma == UI_ALARMA_COLOCACION) {
            // Durante la infusión el click solo silencia (como en las demás alarmas): la alarma se resuelve sola
            // cuando el ángulo vuelve al rango y se sostiene 5 s
            d->alarma_silenciada = true;
            pantalla = UI_PANTALLA_MONITOREO;
        } else if (pulsar && d->colocacion_ok) {
            if (d->alarma == UI_ALARMA_COLOCACION) {
                d->alarma = UI_ALARMA_NINGUNA;   // el usuario confirmó que volvió a colocarlo bien
            }
            if (d->infusion_configurada) {
                pantalla = UI_PANTALLA_MONITOREO;
            } else {
                IrConfig(false);   // primera configuración: no pide código técnico
            }
        }
        break;

    case UI_PANTALLA_ACCESO:
        if (BloqueoRestanteS() > 0) {
            break;   // bloqueada por errores: se ignora todo hasta que termine
        }
        if (sentido) {
            acceso_digitos[acceso_posicion] = (acceso_digitos[acceso_posicion] + 10 + sentido) % 10;
            acceso_error = false;
        } else if (pulsar) {
            if (++acceso_posicion == CODIGO_DIGITOS) {
                if (acceso_arranque && CodigoCoincide(UI_CODIGO_TECNICO)) {
                    // El Modo Técnico solo existe en el arranque normal (sin infusión activa)
                    acceso_errores = 0;
                    acceso_arranque = false;
                    if (clave_cb != NULL) {
                        clave_cb(UI_CLAVE_TECNICO);
                    }
                    IrTecnico();
                } else if (CodigoCoincide(UI_CLAVE_CLINICA)) {
                    acceso_errores = 0;
                    if (clave_cb != NULL) {
                        clave_cb(UI_CLAVE_OK);
                    }
                    if (acceso_arranque) {
                        acceso_arranque = false;
                        pantalla = UI_PANTALLA_COLOCACION;
                    } else if (acceso_origen == UI_PANTALLA_FIN) {
                        AbrirSinInfusion();   // sin infusión: la clave abre la sesión (Nueva infusión o Apagar)
                    } else {
                        IrAjustes();
                    }
                } else {
                    IrAcceso();
                    acceso_error = true;
                    if (clave_cb != NULL) {
                        clave_cb(UI_CLAVE_ERROR);
                    }
                    if (++acceso_errores >= CLAVE_ERRORES_MAX) {
                        acceso_errores = 0;
                        bloqueo_hasta_us = esp_timer_get_time() + (int64_t)t_clave_bloqueo_s * 1000000;
                    }
                }
            }
        } else if (mantener) {
            if (acceso_posicion > 0) {
                acceso_posicion--;   // borra el último dígito
            } else if (!acceso_arranque) {
                pantalla = acceso_origen;   // cancela el acceso (la clave del arranque no se cancela)
            }
        }
        break;

    case UI_PANTALLA_CONFIG:
        if (sentido) {
            CambiarParametro(d, config_indice, sentido);
        } else if (pulsar) {
            if (++config_indice == CONFIG_PARAMETROS) {
                resumen_pagina = 0;
                pantalla = UI_PANTALLA_RESUMEN;
            }
        } else if (mantener) {
            if (config_desde_menu) {
                CancelarEdicion(d);   // cancela el cambio de datos
                IrAjustes();
            } else if (config_indice > 0) {
                config_indice--;
            }
        }
        break;

    case UI_PANTALLA_RESUMEN:
        if (sentido) {
            resumen_pagina = !resumen_pagina;   // el giro alterna entre las dos páginas
        } else if (pulsar && resumen_pagina == 0) {
            resumen_pagina = 1;                 // primero se ve el factor de goteo, después se puede iniciar
        } else if (pulsar) {
            if (MotivoResumen(d) != 0) {
                break;   // menos de 2 gotas/min o memoria llena: no se puede iniciar (hay que cambiar la configuración o llamar a soporte)
            }
            ajuste_es_cambio = config_desde_menu && d->infusion_configurada;   // Cambiar datos: sigue la misma infusión
            ActualizarCaudalObjetivo(d);
            d->infusion_configurada = true;
            d->en_pausa = false;
            d->alarma_silenciada = false;
            pantalla = UI_PANTALLA_AJUSTE_GOTEO;
        } else if (mantener) {
            if (resumen_pagina == 1) {
                resumen_pagina = 0;
            } else {
                IrConfig(config_desde_menu);
            }
        }
        break;

    case UI_PANTALLA_AJUSTE_GOTEO:
        if (pulsar) {
            if (!ajuste_es_cambio) {
                // Empieza el uso: el tiempo y el volumen infundido arrancan de cero recién acá
                d->tiempo_s = 0;
                d->volumen_ml = 0;
            }
            config_desde_menu = false;
            ajuste_es_cambio = false;
            pantalla = UI_PANTALLA_MONITOREO;
        }
        break;

    case UI_PANTALLA_MONITOREO:
        if (muy_largo) {
            IrAcceso();   // ajustes: solo con la clave. Un click o un giro sin alarma no hacen nada
        }
        break;

    case UI_PANTALLA_ALARMA:
        if (pulsar || doble) {   // el usuario solo puede silenciar con el botón (un doble click apurado también vale)
            // También en fin de sachet: solo silencia. La infusión la finaliza la enfermera desde Ajustes > Detener monitoreo
            d->alarma_silenciada = true;
            pantalla = alarma_origen;
        }
        break;

    case UI_PANTALLA_AJUSTES:
        if (sentido) {
            menu_cursor = (menu_cursor + MENU_ITEMS + sentido) % MENU_ITEMS;
            if (menu_cursor < menu_primero) {
                menu_primero = menu_cursor;
            } else if (menu_cursor >= menu_primero + MENU_FILAS_VISIBLES) {
                menu_primero = menu_cursor - MENU_FILAS_VISIBLES + 1;
            }
        } else if (pulsar) {
            if (menu_cursor == 0) {
                duracion_previa = d->duracion_min;   // por si se cancela
                IrConfig(true);
            } else if (menu_cursor == 1) {
                apagar_si = false;
                pantalla = UI_PANTALLA_DETENER;
            } else if (menu_cursor == 2) {
                apagar_si = false;
                apagar_origen = UI_PANTALLA_AJUSTES;
                pantalla = UI_PANTALLA_APAGAR;
            } else {
                pantalla = acceso_origen;   // Volver
            }
        } else if (mantener) {
            pantalla = acceso_origen;   // vuelve al Monitoreo
        }
        break;

    case UI_PANTALLA_DETENER:
        if (sentido) {
            apagar_si = !apagar_si;
        } else if (pulsar) {
            if (apagar_si) {
                FinalizarInfusion(d);   // el main lo ve y deja de medir; queda "Sin infusión activa" con la sesión abierta
                AbrirSinInfusion();
            } else {
                IrAjustes();
            }
        } else if (mantener) {
            IrAjustes();
        }
        break;

    case UI_PANTALLA_FIN:
        if (!sin_infusion_abierta) {
            if (muy_largo) {
                IrAcceso();   // bloqueada: para operar hace falta la clave
            }
        } else if (sentido) {
            menu_cursor = (menu_cursor + SIN_INFUSION_ITEMS + sentido) % SIN_INFUSION_ITEMS;
        } else if (pulsar) {
            if (menu_cursor == 0) {
                sin_infusion_abierta = false;
                pantalla = UI_PANTALLA_COLOCACION;   // infusión nueva: Colocación, Config, Resumen...
            } else {
                apagar_si = false;
                apagar_origen = UI_PANTALLA_FIN;
                pantalla = UI_PANTALLA_APAGAR;
            }
        }
        break;

    case UI_PANTALLA_APAGAR:
        if (sentido) {
            apagar_si = !apagar_si;
        } else if (pulsar || mantener) {
            if (pulsar && apagar_si) {
                if (d->infusion_configurada) {
                    FinalizarInfusion(d);
                }
                apagado_pedido = true;
            } else if (apagar_origen == UI_PANTALLA_FIN) {
                pantalla = UI_PANTALLA_FIN;
            } else {
                IrAjustes();
            }
        }
        break;

    case UI_PANTALLA_TECNICO:
        if (sentido) {
            MoverCursor(&tecnico_cursor, &tecnico_primero, TECNICO_ITEMS, sentido);
        } else if (pulsar) {
            if (tecnico_cursor == 0) {
                pantalla = UI_PANTALLA_DIAGNOSTICO;
            } else if (tecnico_cursor == 1) {
                prueba_cursor = 0;
                prueba_primero = 0;
                d->probar_alarma = 0;
                pantalla = UI_PANTALLA_PROBAR_ALARMA;
            } else if (tecnico_cursor == 2) {
                apagar_si = false;
                pantalla = UI_PANTALLA_BORRAR_REGISTRO;
            } else {
                salir_pedido = true;   // Salir: el main reinicia el equipo (logo, autotest y clave)
            }
        }
        break;

    case UI_PANTALLA_DIAGNOSTICO:
        if (pulsar || mantener) {
            pantalla = UI_PANTALLA_TECNICO;
        }
        break;

    case UI_PANTALLA_PROBAR_ALARMA:
        if (sentido) {
            d->probar_alarma = 0;   // al cambiar de alarma deja de sonar la anterior
            MoverCursor(&prueba_cursor, &prueba_primero, PRUEBA_ITEMS, sentido);
        } else if (pulsar) {
            d->probar_alarma = (d->probar_alarma == prueba_cursor + 1) ? 0 : prueba_cursor + 1;
        } else if (mantener) {
            d->probar_alarma = 0;
            pantalla = UI_PANTALLA_TECNICO;
        }
        break;

    case UI_PANTALLA_BORRAR_REGISTRO:
        if (sentido) {
            apagar_si = !apagar_si;
        } else if (pulsar) {
            d->borrar_registro = apagar_si;   // el main borra el registro
            pantalla = UI_PANTALLA_TECNICO;
        } else if (mantener) {
            pantalla = UI_PANTALLA_TECNICO;
        }
        break;
    }

    if (!apagado_pedido) {
        DibujarPantalla(d);
    }
}
