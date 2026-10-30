/*==================[inclusions]=============================================*/
#include <math.h>
#include <string.h>
#include "alarmas.h"

/*==================[macros and definitions]=================================*/
#define US(s)                   ((int64_t)(s) * 1000000LL)

/* Flujo detenido (sección 1 de la especificación) */
#define OCL_N                   3.0f      /*!< T_ocl = N * T_obj: tolera hasta 2 gotas seguidas no detectadas */
#define OCL_T_MIN_S             15.0f
#define OCL_T_MAX_S             120.0f
#define OCL_GOTAS_RESOLVER      3         /*!< Gotas seguidas para que la alarma se resuelva sola */
#define GOTAS_MIN_OBJETIVO      2.0f      /*!< Menos que esto no se permite iniciar */

/* Desviación de flujo (sección 2) */
#define DESV_INTERVALOS         10        /*!< Intervalos entre gotas con los que se calcula la mediana */
#define DESV_INTERVALOS_MIN     5         /*!< Mínimo de intervalos para evaluar (a caudales bajos llenar 10 tarda minutos) */
#define DESV_ESTABILIZACION_S   120       /*!< No se evalúa en los primeros 2 min del Monitoreo */
#define DESV_LEVE_ON            0.10f
#define DESV_LEVE_OFF           0.08f
#define DESV_LEVE_PERSIST_S     300
#define DESV_FUERTE_ON          0.25f
#define DESV_FUERTE_OFF         0.23f
#define DESV_FUERTE_PERSIST_S   120

/* Fin de infusión (sección 3) */
#define FIN_PRE_AVISO           0.90f     /*!< Fracción del volumen programado: pre-aviso y umbral de fin de sachet (provisorio) */

/* Batería (sección 4) */
#define BAT_AVISO_PCT           20
#define BAT_BAJA_PCT            10
#define BAT_CRITICA_PCT         5
#define BAT_HISTERESIS_PCT      2

/* Colocación (sección 5) */
#define COL_PERSIST_S           5         /*!< Tiempo que la condición tiene que sostenerse para activarse o apagarse */

/* Silenciado (sección 0) */
#define REARME_S                120

/*==================[internal data definition]===============================*/
static uint16_t factor = 20;                /* gotas por mL */
static uint16_t vol_prog_ml = 0;
static float t_obj_s = 1.0f;                /* período objetivo entre gotas */
static float t_ocl_s = OCL_T_MIN_S;
static uint32_t gotas = 0;

static bool hay_gota = false;
static int64_t ultima_gota_us = 0;
static int64_t intervalos_us[DESV_INTERVALOS];  /* buffer circular de los últimos intervalos */
static uint8_t n_intervalos = 0;
static uint8_t sig_intervalo = 0;

static bool en_monitoreo_ant = false;
static int64_t monitoreo_ini_us = 0;
static int64_t ref_ocl_us = 0;              /* la espera de T_ocl se cuenta desde la última gota o desde esta hora, la que sea más tarde */
static bool flujo_latch = false;            /* hay flujo detenido o fin de sachet (se resuelve con OCL_GOTAS_RESOLVER gotas seguidas) */
static uint8_t gotas_seguidas = 0;

static float desvio = 0.0f;
static int64_t desv_leve_ini_us = -1;
static int64_t desv_fuerte_ini_us = -1;

static bool col_activa = false;
static int64_t col_cambio_ini_us = -1;
static bool vol_incierto = false;

static uint8_t bat_nivel = 0;               /* 0 = normal, 1 = aviso, 2 = baja, 3 = crítica */

static uint32_t activas = 0;
static uint32_t silenciadas = 0;
static int64_t rearme_us[ALARMAS_CANTIDAD];
static alarmas_cb_t callback = NULL;

static const char *const nombres[ALARMAS_CANTIDAD] = {
    "VOLUMEN_COMPLETO", "FLUJO_DETENIDO", "FIN_SACHET", "COLOCACION", "BATERIA_CRITICA",
    "DESVIACION_FUERTE", "BATERIA_BAJA", "AVISO_DESVIACION_LEVE", "AVISO_PROXIMO_A_FINALIZAR",
    "AVISO_BATERIA"};

/*==================[internal functions definition]==========================*/
static float GotasMinObjetivo(uint16_t fg, uint16_t vol, uint16_t dur_min) {
    return (dur_min > 0) ? (float)vol * fg / dur_min : 0.0f;
}

/** Activa o apaga un evento; avisa al callback solo si cambió. */
static void Poner(alarma_evento_t e, bool on, int64_t ahora_us) {
    bool ahora_on = (activas & ALARMA_BIT(e)) != 0;
    if (on == ahora_on) {
        return;
    }
    if (on) {
        activas |= ALARMA_BIT(e);
    } else {
        activas &= ~ALARMA_BIT(e);
        silenciadas &= ~ALARMA_BIT(e);
    }
    if (callback != NULL) {
        callback(e, on, ahora_us);
    }
}

static float VolumenInfundido(void) {
    return (float)gotas / factor;
}

/** Mediana de los intervalos guardados, en segundos. */
static float MedianaS(void) {
    int64_t v[DESV_INTERVALOS];
    memcpy(v, intervalos_us, sizeof(v));
    for (int i = 1; i < n_intervalos; i++) {   // ordenamiento por inserción (son 10 como máximo)
        int64_t x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }
    int64_t med = (n_intervalos % 2) ? v[n_intervalos / 2]
                                     : (v[n_intervalos / 2 - 1] + v[n_intervalos / 2]) / 2;
    return (float)med / 1e6f;
}

static void ReiniciarIntervalos(void) {
    n_intervalos = 0;
    sig_intervalo = 0;
}

static int MasPrioritario(uint32_t mascara) {
    for (int e = 0; e < ALARMAS_CANTIDAD; e++) {
        if (mascara & ALARMA_BIT(e)) {
            return e;
        }
    }
    return -1;
}

/** Tiempo que lleva sostenida una condición. Guarda el instante de inicio en *ini_us (-1 = no se cumple). */
static bool Sostenida(bool condicion, int64_t *ini_us, int64_t ahora_us, int64_t persist_us) {
    if (!condicion) {
        *ini_us = -1;
        return false;
    }
    if (*ini_us < 0) {
        *ini_us = ahora_us;
    }
    return (ahora_us - *ini_us) >= persist_us;
}

static void EvaluarColocacion(const alarmas_entrada_t *e, int64_t ahora_us) {
    bool mala = (fabsf(e->angulo_grados) > e->tolerancia_grados) || !e->base_optica_ok;
    // Para activarse la condición mala tiene que sostenerse; para apagarse, la buena.
    bool cambio = Sostenida(col_activa ? !mala : mala, &col_cambio_ini_us, ahora_us, US(COL_PERSIST_S));
    if (cambio) {
        col_activa = !col_activa;
        col_cambio_ini_us = -1;
        if (col_activa) {
            vol_incierto = true;
        }
    }
    Poner(ALARMA_COLOCACION, col_activa, ahora_us);
}

static void EvaluarFlujo(int64_t ahora_us) {
    int64_t ultima = (hay_gota && ultima_gota_us > ref_ocl_us) ? ultima_gota_us : ref_ocl_us;
    if (!flujo_latch && (ahora_us - ultima) > (int64_t)(t_ocl_s * 1e6f)) {
        flujo_latch = true;
        gotas_seguidas = 0;
    }
    bool fin = flujo_latch && VolumenInfundido() >= FIN_PRE_AVISO * vol_prog_ml;
    Poner(ALARMA_FLUJO_DETENIDO, flujo_latch && !fin, ahora_us);
    Poner(ALARMA_FIN_SACHET, fin, ahora_us);
}

static void EvaluarDesvio(int64_t ahora_us) {
    if (n_intervalos < DESV_INTERVALOS_MIN) {
        return;
    }
    float t_med = MedianaS();
    if (t_med <= 0.0f) {
        return;
    }
    desvio = (t_obj_s / t_med) - 1.0f;   // positivo = más rápido que el objetivo
    float ad = fabsf(desvio);

    bool leve = (activas & ALARMA_BIT(AVISO_DESVIACION_LEVE)) != 0;
    if (leve) {
        if (ad < DESV_LEVE_OFF) {
            Poner(AVISO_DESVIACION_LEVE, false, ahora_us);
            desv_leve_ini_us = -1;
        }
    } else if (Sostenida(ad > DESV_LEVE_ON, &desv_leve_ini_us, ahora_us, US(DESV_LEVE_PERSIST_S))) {
        Poner(AVISO_DESVIACION_LEVE, true, ahora_us);
    }

    bool fuerte = (activas & ALARMA_BIT(ALARMA_DESVIACION_FUERTE)) != 0;
    if (fuerte) {
        if (ad < DESV_FUERTE_OFF) {
            Poner(ALARMA_DESVIACION_FUERTE, false, ahora_us);
            desv_fuerte_ini_us = -1;
        }
    } else if (Sostenida(ad > DESV_FUERTE_ON, &desv_fuerte_ini_us, ahora_us, US(DESV_FUERTE_PERSIST_S))) {
        Poner(ALARMA_DESVIACION_FUERTE, true, ahora_us);
    }
}

static void EvaluarBateria(const alarmas_entrada_t *e, int64_t ahora_us) {
    static const uint8_t umbral[4] = {0, BAT_AVISO_PCT, BAT_BAJA_PCT, BAT_CRITICA_PCT};
    if (e->cargador) {
        bat_nivel = 0;
    } else {
        // Mejora: baja de escalón solo si supera el umbral + histéresis
        while (bat_nivel > 0 && e->bateria_pct > umbral[bat_nivel] + BAT_HISTERESIS_PCT) {
            bat_nivel--;
        }
        // Empeora: entra al escalón apenas llega al umbral
        for (uint8_t n = 3; n > bat_nivel; n--) {
            if (e->bateria_pct <= umbral[n]) {
                bat_nivel = n;
                break;
            }
        }
    }
    Poner(ALARMA_BATERIA_CRITICA, bat_nivel == 3, ahora_us);
    Poner(ALARMA_BATERIA_BAJA, bat_nivel == 2, ahora_us);
    Poner(AVISO_BATERIA, bat_nivel == 1, ahora_us);
}

/*==================[external functions definition]==========================*/
bool Alarmas_ResumenValido(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min) {
    return GotasMinObjetivo(factor_goteo, volumen_ml, duracion_min) >= GOTAS_MIN_OBJETIVO;
}

void Alarmas_Iniciar(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min, int64_t ahora_us) {
    factor = (factor_goteo > 0) ? factor_goteo : 20;
    vol_prog_ml = volumen_ml;
    float gmin = GotasMinObjetivo(factor, volumen_ml, duracion_min);
    t_obj_s = (gmin > 0.0f) ? 60.0f / gmin : OCL_T_MAX_S;
    t_ocl_s = OCL_N * t_obj_s;
    if (t_ocl_s < OCL_T_MIN_S) {
        t_ocl_s = OCL_T_MIN_S;
    }
    if (t_ocl_s > OCL_T_MAX_S) {
        t_ocl_s = OCL_T_MAX_S;
    }

    gotas = 0;
    hay_gota = false;
    ultima_gota_us = 0;
    ReiniciarIntervalos();
    en_monitoreo_ant = false;
    monitoreo_ini_us = ahora_us;
    ref_ocl_us = ahora_us;
    flujo_latch = false;
    gotas_seguidas = 0;
    desvio = 0.0f;
    desv_leve_ini_us = -1;
    desv_fuerte_ini_us = -1;
    col_activa = false;
    col_cambio_ini_us = -1;
    vol_incierto = false;
    bat_nivel = 0;
    activas = 0;
    silenciadas = 0;
    memset(rearme_us, 0, sizeof(rearme_us));
}

void Alarmas_Gota(int64_t ahora_us) {
    int64_t intervalo_us = hay_gota ? ahora_us - ultima_gota_us : 0;
    gotas++;

    if (flujo_latch) {
        // Cuenta gotas seguidas: si pasó más de T_ocl desde la anterior, vuelve a empezar
        gotas_seguidas = (intervalo_us > (int64_t)(t_ocl_s * 1e6f)) ? 1 : gotas_seguidas + 1;
        if (gotas_seguidas >= OCL_GOTAS_RESOLVER) {
            flujo_latch = false;
            gotas_seguidas = 0;
            ReiniciarIntervalos();
            Poner(ALARMA_FLUJO_DETENIDO, false, ahora_us);
            Poner(ALARMA_FIN_SACHET, false, ahora_us);
        }
    } else if (intervalo_us > 0) {
        if (intervalo_us > (int64_t)(t_ocl_s * 1e6f)) {
            ReiniciarIntervalos();   // fue una pausa, no un intervalo de goteo
        } else {
            intervalos_us[sig_intervalo] = intervalo_us;
            sig_intervalo = (sig_intervalo + 1) % DESV_INTERVALOS;
            if (n_intervalos < DESV_INTERVALOS) {
                n_intervalos++;
            }
        }
    }
    hay_gota = true;
    ultima_gota_us = ahora_us;
}

void Alarmas_Actualizar(const alarmas_entrada_t *e, int64_t ahora_us) {
    EvaluarBateria(e, ahora_us);

    if (!e->en_monitoreo) {
        // Fuera del Monitoreo no se evalúa nada más y se apagan los eventos de la infusión
        en_monitoreo_ant = false;
        flujo_latch = false;
        col_activa = false;
        col_cambio_ini_us = -1;
        desv_leve_ini_us = -1;
        desv_fuerte_ini_us = -1;
        static const alarma_evento_t de_infusion[] = {
            ALARMA_VOLUMEN_COMPLETO, ALARMA_FLUJO_DETENIDO, ALARMA_FIN_SACHET, ALARMA_COLOCACION,
            ALARMA_DESVIACION_FUERTE, AVISO_DESVIACION_LEVE, AVISO_PROXIMO_A_FINALIZAR};
        for (size_t i = 0; i < sizeof(de_infusion) / sizeof(de_infusion[0]); i++) {
            Poner(de_infusion[i], false, ahora_us);
        }
        return;
    }

    if (!en_monitoreo_ant) {   // empieza el Monitoreo: el tiempo y la espera de T_ocl se cuentan desde acá
        en_monitoreo_ant = true;
        monitoreo_ini_us = ahora_us;
        ref_ocl_us = ahora_us;
        ReiniciarIntervalos();
    }

    EvaluarColocacion(e, ahora_us);

    bool suspendida = col_activa || e->en_pausa;
    if (suspendida) {
        // Sin deteccion de flujo ni desvío; la espera de T_ocl se reinicia para no disparar al volver
        ref_ocl_us = ahora_us;
        desv_leve_ini_us = -1;
        desv_fuerte_ini_us = -1;
    } else {
        EvaluarFlujo(ahora_us);
        if (!flujo_latch && (ahora_us - monitoreo_ini_us) >= US(DESV_ESTABILIZACION_S)) {
            EvaluarDesvio(ahora_us);
        } else {
            desv_leve_ini_us = -1;
            desv_fuerte_ini_us = -1;
        }
    }

    float v = VolumenInfundido();
    Poner(ALARMA_VOLUMEN_COMPLETO, !flujo_latch && vol_prog_ml > 0 && v >= vol_prog_ml, ahora_us);
    Poner(AVISO_PROXIMO_A_FINALIZAR, vol_prog_ml > 0 && v >= FIN_PRE_AVISO * vol_prog_ml, ahora_us);

    // Las alarmas altas silenciadas se re-arman a los 2 min si la condición sigue activa
    uint32_t re = silenciadas & ALARMAS_ALTAS_MASK;
    for (int i = 0; i < ALARMAS_CANTIDAD; i++) {
        if ((re & ALARMA_BIT(i)) && ahora_us >= rearme_us[i]) {
            silenciadas &= ~ALARMA_BIT(i);
        }
    }
}

void Alarmas_SetCallback(alarmas_cb_t cb) {
    callback = cb;
}

void Alarmas_Silenciar(int64_t ahora_us) {
    int e = Alarmas_Mostrar();
    if (e < 0) {
        return;
    }
    silenciadas |= ALARMA_BIT(e);
    rearme_us[e] = ahora_us + US(REARME_S);
}

uint32_t Alarmas_Activas(void) {
    return activas;
}

uint32_t Alarmas_Silenciadas(void) {
    return silenciadas;
}

int Alarmas_Mostrar(void) {
    return MasPrioritario(activas & ALARMAS_SONORAS_MASK & ~silenciadas);
}

int Alarmas_Pie(void) {
    uint32_t avisos = activas & ~ALARMAS_SONORAS_MASK;
    return MasPrioritario(avisos | (activas & silenciadas));
}

float Alarmas_Desvio(void) {
    return desvio;
}

uint16_t Alarmas_VolumenMl(void) {
    uint32_t ml = (gotas + factor / 2) / factor;
    return (ml > UINT16_MAX) ? UINT16_MAX : (uint16_t)ml;
}

bool Alarmas_VolumenIncierto(void) {
    return vol_incierto;
}

float Alarmas_TOclusionS(void) {
    return t_ocl_s;
}

const char *Alarmas_Nombre(alarma_evento_t evento) {
    return (evento < ALARMAS_CANTIDAD) ? nombres[evento] : "?";
}
