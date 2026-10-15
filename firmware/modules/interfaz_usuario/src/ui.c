/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <string.h>
#include "esp_idf_version.h"   // lo necesita u8g2_esp32_hal.h para elegir la API de I2C
#include "ui.h"
#include "ui_pantallas.h"
#include "u8g2_esp32_hal.h"
#include "refresh_wait_32_32_28f.h"

/*==================[macros and definitions]=================================*/
#define OLED_SDA   21
#define OLED_SCL   7
#define SSD1315_ANCHO  128
#define OLED_ADDR  0x78   /* 0x3C corrido un bit a la izquierda (formato de u8g2) */

#define UI_ALARMA_PARPADEA   0    /*!< 1 hace que la pantalla de alarma alterne entre fondo blanco y negro */
#define CODIGO_DIGITOS       4
#define CONFIG_PARAMETROS    3
#define MENU_FILAS_VISIBLES  3
#define MENU_ITEMS           2   /*!< Menú clínico: Configurar y Apagar equipo */

/*==================[internal data definition]===============================*/
static u8g2_t u8g2;

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
    {"Tiempo total",     "h",      NULL,           0,                                                  30, 1440, 30},   // en minutos, de a media hora
};

/* Estado de la navegación */
static ui_pantalla_t pantalla = UI_PANTALLA_AUTOTEST;
static const ui_chequeo_t *chequeos_autotest = NULL;
static uint8_t chequeos_cantidad = 0;
static uint8_t acceso_digitos[CODIGO_DIGITOS];
static uint8_t acceso_posicion = 0;
static bool acceso_error = false;
static uint8_t config_indice = 0;
static uint8_t menu_cursor = 0;
static uint8_t menu_primero = 0;
static bool apagar_si = false;
static ui_pantalla_t acceso_origen = UI_PANTALLA_MONITOREO;   // pantalla a la que se vuelve si se cancela el acceso
static bool config_desde_menu = false;   // true si Config se abrió desde el Menú técnico (y no en la primera configuración)
static bool apagado_pedido = false;
static bool parpadeo = false;
static ui_alarma_t ultima_alarma = UI_ALARMA_NINGUNA;

/*==================[internal functions definition]==========================*/
static void PantallaColocacion(const ui_datos_t *d) {
    char texto[32];

    u8g2_ClearBuffer(&u8g2);
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

/* Texto del aviso que queda abajo en el Monitoreo cuando hay una alarma silenciada o pausa */
static const char *TextoAviso(const ui_datos_t *d) {
    if (d->alarma_silenciada) {
        switch (d->alarma) {
        case UI_ALARMA_OCLUSION:         return "! OCLUSION";
        case UI_ALARMA_DESVIACION_FLUJO: return "! DESVIACION FLUJO";
        case UI_ALARMA_FIN_SACHET:       return "! FIN DE SACHET";
        case UI_ALARMA_BATERIA_BAJA:     return "! BATERIA BAJA";
        default: break;
        }
    }
    return d->en_pausa ? "|| PAUSA" : NULL;
}

static void PantallaMonitoreo(const ui_datos_t *d) {
    char texto[16];
    const char *aviso = TextoAviso(d);

    u8g2_ClearBuffer(&u8g2);
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
    snprintf(texto, sizeof(texto), "%uml", d->volumen_ml);
    u8g2_DrawStr(&u8g2, 92, 30, texto);
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
        u8g2_DrawStr(&u8g2, (SSD1315_ANCHO - u8g2_GetStrWidth(&u8g2, "Doble click: menu")) / 2, 61, "Doble click: menu");
    }

    u8g2_SendBuffer(&u8g2);
}

static void PantallaAlarma(const ui_datos_t *d) {
    char detalle[32];
    const char *nivel = "ALARMA ALTA", *titulo = "";
    int pct;

    switch (d->alarma) {
    case UI_ALARMA_OCLUSION:
        titulo = "OCLUSION";
        snprintf(detalle, sizeof(detalle), "Sin gotas hace %u s", d->segundos_sin_gotas);
        break;
    case UI_ALARMA_DESVIACION_FLUJO:
        nivel = "ALARMA MEDIA";
        titulo = (d->flujo_mlh > d->flujo_objetivo_mlh) ? "FLUJO ALTO" : "FLUJO BAJO";
        pct = d->flujo_objetivo_mlh ? ((int)d->flujo_mlh - (int)d->flujo_objetivo_mlh) * 100 / d->flujo_objetivo_mlh : 0;
        snprintf(detalle, sizeof(detalle), "%+d%% (obj %u mL/h)", pct, d->flujo_objetivo_mlh);
        break;
    case UI_ALARMA_FIN_SACHET:
        titulo = "FIN DE SACHET";
        snprintf(detalle, sizeof(detalle), "Volumen %u mL completo", d->volumen_total_ml);
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
    Pant_Alarma(&u8g2, UI_ALARMA_PARPADEA ? parpadeo : true, nivel, titulo, detalle);
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
        snprintf(texto, tam, (valor % 60) ? "%u.5" : "%u", valor / 60);
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

static void IrAcceso(void) {
    if (pantalla != UI_PANTALLA_ACCESO) {
        acceso_origen = pantalla;   // desde el Monitoreo o desde Fin de infusión
    }
    memset(acceso_digitos, 0, sizeof(acceso_digitos));
    acceso_posicion = 0;
    acceso_error = false;
    pantalla = UI_PANTALLA_ACCESO;
}

static void IrMenuTecnico(void) {
    menu_cursor = 0;
    menu_primero = 0;
    pantalla = UI_PANTALLA_MENU_TECNICO;
}

static void IrConfig(bool desde_menu) {
    config_desde_menu = desde_menu;
    config_indice = 0;
    pantalla = UI_PANTALLA_CONFIG;
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
    // El doble click se usa solo donde hace falta: así los clicks del resto no esperan
    Encoder_SetDobleClick(pantalla == UI_PANTALLA_MONITOREO || pantalla == UI_PANTALLA_ALARMA ||
                          pantalla == UI_PANTALLA_FIN);
    char valor[12];
    const char *items[MENU_ITEMS] = {"Configurar", "Apagar equipo"};

    switch (pantalla) {
    case UI_PANTALLA_AUTOTEST:
        Pant_Autotest(&u8g2, d->bateria_pct, chequeos_autotest, chequeos_cantidad, TodosOk());
        break;
    case UI_PANTALLA_COLOCACION:
        PantallaColocacion(d);
        break;
    case UI_PANTALLA_ACCESO:
        Pant_Acceso(&u8g2, acceso_digitos, acceso_posicion, acceso_error);
        break;
    case UI_PANTALLA_CONFIG:
        ValorTexto((ui_datos_t *)d, config_indice, valor, sizeof(valor));
        Pant_Config(&u8g2, config_indice + 1, CONFIG_PARAMETROS, parametros[config_indice].nombre, valor, parametros[config_indice].unidad);
        break;
    case UI_PANTALLA_RESUMEN:
        Pant_Resumen(&u8g2, d);
        break;
    case UI_PANTALLA_AJUSTE_GOTEO:
        Pant_AjusteGoteo(&u8g2, GotasMinObjetivo(d), d->volumen_total_ml, d->duracion_min, d->factor_goteo);
        break;
    case UI_PANTALLA_MONITOREO:
        PantallaMonitoreo(d);
        break;
    case UI_PANTALLA_ALARMA:
        PantallaAlarma(d);
        break;
    case UI_PANTALLA_MENU_TECNICO:
        Pant_Menu(&u8g2, "Menu tecnico", items, MENU_ITEMS, menu_cursor, menu_primero, "Pulsar: elegir");
        break;
    case UI_PANTALLA_FIN:
        Pant_Fin(&u8g2, d->bateria_pct, d->volumen_ml, d->tiempo_s);
        break;
    case UI_PANTALLA_APAGAR:
        Pant_Apagar(&u8g2, apagar_si);
        break;
    }
}

/*==================[external functions definition]==========================*/
bool UI_Init(void) {
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = OLED_SDA;
    hal.bus.i2c.scl = OLED_SCL;
    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, OLED_ADDR);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
    return true;
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

void UI_Actualizar(ui_datos_t *d) {
    parpadeo = !parpadeo;

    // Una alarma distinta de la anterior vuelve a sonar aunque se hubiera silenciado la anterior
    if (d->alarma != ultima_alarma) {
        d->alarma_silenciada = false;
        ultima_alarma = d->alarma;
    }

    if (d->alarma == UI_ALARMA_NINGUNA) {
        d->alarma_silenciada = false;
        if (pantalla == UI_PANTALLA_ALARMA) {
            pantalla = UI_PANTALLA_MONITOREO;
        }
    } else if (d->infusion_configurada) {
        bool en_infusion = (pantalla == UI_PANTALLA_AJUSTE_GOTEO) || (pantalla == UI_PANTALLA_MONITOREO) ||
                           (pantalla == UI_PANTALLA_MENU_TECNICO) || (pantalla == UI_PANTALLA_ALARMA) ||
                           (pantalla == UI_PANTALLA_APAGAR) || (pantalla == UI_PANTALLA_COLOCACION);
        if (d->alarma == UI_ALARMA_COLOCACION) {
            if (en_infusion) {
                pantalla = UI_PANTALLA_COLOCACION;   // un choque movió el equipo: se vuelve a verificar
            }
        } else if (!d->alarma_silenciada && en_infusion) {
            pantalla = UI_PANTALLA_ALARMA;
        }
    }
    DibujarPantalla(d);
}

void UI_Evento(encoder_evento_t ev, ui_datos_t *d) {
    int8_t sentido = (ev == ENC_GIRO_HORARIO) ? 1 : (ev == ENC_GIRO_ANTIHORARIO) ? -1 : 0;
    bool pulsar = (ev == ENC_CLICK), mantener = (ev == ENC_CLICK_LARGO), doble = (ev == ENC_DOBLE_CLICK);

    switch (pantalla) {
    case UI_PANTALLA_AUTOTEST:
        if (pulsar && TodosOk()) {
            pantalla = UI_PANTALLA_COLOCACION;
        }
        break;

    case UI_PANTALLA_COLOCACION:
        if (pulsar && d->colocacion_ok) {
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
        if (sentido) {
            acceso_digitos[acceso_posicion] = (acceso_digitos[acceso_posicion] + 10 + sentido) % 10;
            acceso_error = false;
        } else if (pulsar) {
            if (++acceso_posicion == CODIGO_DIGITOS) {
                bool correcto = true;
                for (uint8_t i = 0; i < CODIGO_DIGITOS; i++) {
                    correcto = correcto && (acceso_digitos[i] == UI_CODIGO_TECNICO[i] - '0');
                }
                if (correcto) {
                    IrMenuTecnico();
                } else {
                    IrAcceso();
                    acceso_error = true;
                }
            }
        } else if (mantener) {
            if (acceso_posicion > 0) {
                acceso_posicion--;   // borra el último dígito
            } else {
                pantalla = acceso_origen;   // cancela el acceso
            }
        }
        break;

    case UI_PANTALLA_CONFIG:
        if (sentido) {
            CambiarParametro(d, config_indice, sentido);
        } else if (pulsar) {
            if (++config_indice == CONFIG_PARAMETROS) {
                pantalla = UI_PANTALLA_RESUMEN;
            }
        } else if (mantener) {
            if (config_indice > 0) {
                config_indice--;
            } else if (config_desde_menu) {
                IrMenuTecnico();
            }
        }
        break;

    case UI_PANTALLA_RESUMEN:
        if (pulsar) {
            ActualizarCaudalObjetivo(d);
            d->infusion_configurada = true;
            d->en_pausa = false;
            d->alarma_silenciada = false;
            pantalla = UI_PANTALLA_AJUSTE_GOTEO;
        } else if (mantener) {
            IrConfig(config_desde_menu);
        }
        break;

    case UI_PANTALLA_AJUSTE_GOTEO:
        if (pulsar) {
            // Empieza el uso: el tiempo y el volumen infundido arrancan de cero recién acá
            d->tiempo_s = 0;
            d->volumen_ml = 0;
            pantalla = UI_PANTALLA_MONITOREO;
        }
        break;

    case UI_PANTALLA_MONITOREO:
        if (doble) {
            IrAcceso();   // ajustes: solo con el código técnico
        }
        break;

    case UI_PANTALLA_ALARMA:
        if (pulsar || doble) {   // el usuario solo puede silenciar con el botón (un doble click apurado también vale)
            if (d->alarma == UI_ALARMA_FIN_SACHET) {
                d->infusion_configurada = false;   // terminó la infusión
                d->alarma = UI_ALARMA_NINGUNA;
                pantalla = UI_PANTALLA_FIN;
            } else {
                d->alarma_silenciada = true;
                pantalla = UI_PANTALLA_MONITOREO;
            }
        }
        break;

    case UI_PANTALLA_MENU_TECNICO:
        if (sentido) {
            menu_cursor = (menu_cursor + MENU_ITEMS + sentido) % MENU_ITEMS;
            if (menu_cursor < menu_primero) {
                menu_primero = menu_cursor;
            } else if (menu_cursor >= menu_primero + MENU_FILAS_VISIBLES) {
                menu_primero = menu_cursor - MENU_FILAS_VISIBLES + 1;
            }
        } else if (pulsar) {
            if (menu_cursor == 0) {
                IrConfig(true);
            } else {
                apagar_si = false;
                pantalla = UI_PANTALLA_APAGAR;
            }
        } else if (mantener) {
            pantalla = acceso_origen;   // vuelve al Monitoreo o a Fin de infusión
        }
        break;

    case UI_PANTALLA_FIN:
        if (doble) {
            IrAcceso();
        }
        break;

    case UI_PANTALLA_APAGAR:
        if (sentido) {
            apagar_si = !apagar_si;
        } else if (pulsar) {
            if (apagar_si) {
                apagado_pedido = true;
            } else {
                IrMenuTecnico();
            }
        } else if (mantener) {
            IrMenuTecnico();
        }
        break;
    }

    if (!apagado_pedido) {
        DibujarPantalla(d);
    }
}
