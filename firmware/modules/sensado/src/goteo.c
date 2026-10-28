/*==================[inclusions]=============================================*/
#include <string.h>
#include "goteo.h"

/*==================[internal data definition]===============================*/
static uint16_t factor = 20;                 /* gotas por mL */
static uint32_t gotas = 0;                   /* gotas contadas desde el inicio */
static int64_t t_inicio_us = 0;
static int64_t horas_us[GOTEO_VENTANA];      /* hora de las últimas gotas (buffer circular) */
static uint8_t cantidad = 0;                 /* cuántas hay cargadas en horas_us */
static uint8_t siguiente = 0;                /* dónde se guarda la próxima */

/*==================[internal functions definition]==========================*/
static int64_t UltimaGota(void) {
    return horas_us[(siguiente + GOTEO_VENTANA - 1) % GOTEO_VENTANA];
}

/*==================[external functions definition]==========================*/
void Goteo_Iniciar(uint16_t factor_goteo, int64_t ahora_us) {
    factor = (factor_goteo > 0) ? factor_goteo : 20;
    gotas = 0;
    t_inicio_us = ahora_us;
    cantidad = 0;
    siguiente = 0;
    memset(horas_us, 0, sizeof(horas_us));
}

void Goteo_ReiniciarTiempo(int64_t ahora_us) {
    t_inicio_us = ahora_us;
}

void Goteo_Gota(int64_t ahora_us) {
    gotas++;
    horas_us[siguiente] = ahora_us;
    siguiente = (siguiente + 1) % GOTEO_VENTANA;
    if (cantidad < GOTEO_VENTANA) {
        cantidad++;
    }
}

uint32_t Goteo_Gotas(void) {
    return gotas;
}

uint16_t Goteo_VolumenMl(void) {
    uint32_t ml = (gotas + factor / 2) / factor;
    return (ml > UINT16_MAX) ? UINT16_MAX : (uint16_t)ml;
}

uint16_t Goteo_CaudalMlh(int64_t ahora_us) {
    if (cantidad < 2) {
        return 0;
    }
    int64_t ultima = UltimaGota();
    int64_t primera = horas_us[(siguiente + GOTEO_VENTANA - cantidad) % GOTEO_VENTANA];
    int64_t desde_ultima = ahora_us - ultima;
    if (desde_ultima > GOTEO_CAUDAL_CERO_US) {
        return 0;
    }

    // Tiempo medio entre gotas; si la última fue hace más que eso, las gotas se están espaciando
    int64_t intervalo_us = (ultima - primera) / (cantidad - 1);
    if (desde_ultima > intervalo_us) {
        intervalo_us = desde_ultima;
    }
    if (intervalo_us <= 0) {
        return 0;
    }

    // mL/h = 3600 s / (intervalo en s x gotas por mL) = 3600e6 / (intervalo en us x gotas por mL)
    int64_t divisor = intervalo_us * factor;
    int64_t mlh = (3600000000LL + divisor / 2) / divisor;
    return (mlh > UINT16_MAX) ? UINT16_MAX : (uint16_t)mlh;
}

uint32_t Goteo_TiempoS(int64_t ahora_us) {
    return (ahora_us > t_inicio_us) ? (uint32_t)((ahora_us - t_inicio_us) / 1000000) : 0;
}

uint16_t Goteo_SegundosSinGotas(int64_t ahora_us) {
    // Si todavía no cayó ninguna gota (o la última fue antes de reiniciar el tiempo), se cuenta desde t_inicio_us
    int64_t ultima_us = (cantidad > 0 && UltimaGota() > t_inicio_us) ? UltimaGota() : t_inicio_us;
    int64_t desde_us = ahora_us - ultima_us;
    int64_t s = (desde_us > 0) ? desde_us / 1000000 : 0;
    return (s > UINT16_MAX) ? UINT16_MAX : (uint16_t)s;
}
