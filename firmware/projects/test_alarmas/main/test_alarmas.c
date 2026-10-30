/*! @mainpage Test alarmas
 *
 * @section genDesc General Description
 *
 * Verifica la lógica del componente alarmas con un reloj simulado: genera gotas y avanza el tiempo
 * segundo a segundo (en pocos segundos reales simula horas) y comprueba qué eventos se activan y
 * cuándo, según la especificación de alarmas. Imprime cada activación y desactivación con la hora
 * simulada y al final un resumen PASS/FALLA. No usa sensores ni buzzer.
 *
 * Casos: validación del Resumen, T_ocl, goteo normal sin eventos, flujo detenido y su resolución con
 * 3 gotas, desviación leve y fuerte con persistencia e histéresis, colocación (inclinación y base
 * óptica) con su persistencia y sin falsos flujos detenidos, escalones de batería con histéresis y
 * cargador, pre-aviso/fin de sachet/volumen completo, y prioridad, silenciado y re-armado a los
 * 2 min.
 *
 * Configuración de los casos: 500 mL en 8 h con 20 gtt/mL (20,8 gotas/min, una gota cada 2,88 s,
 * T_ocl = 15 s).
 *
 * @note La salida va por USB Serial/JTAG (sdkconfig.defaults). El programa termina imprimiendo
 * "FIN" y se queda esperando.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alarmas.h"

/*==================[macros and definitions]=================================*/
#define T_OBJ_S    2.88f      /* período objetivo de la configuración de los casos */

#define CHECK(cond, texto) do { \
        if (cond) { pass++; printf("  PASS  %s\n", texto); } \
        else { fail++; printf("  FALLA %s\n", texto); } \
    } while (0)

/*==================[internal data definition]===============================*/
static int64_t t_us = 0;             /* reloj simulado */
static int64_t prox_gota_us = -1;
static alarmas_entrada_t ent;
static int pass = 0, fail = 0;

/*==================[internal functions definition]==========================*/
static void OnEvento(alarma_evento_t e, bool activo, int64_t ahora_us)
{
    printf("      [%7.1f s] %c %s\n", ahora_us / 1e6, activo ? '+' : '-', Alarmas_Nombre(e));
}

static bool Activa(alarma_evento_t e)
{
    return (Alarmas_Activas() & ALARMA_BIT(e)) != 0;
}

static void EntradaNormal(void)
{
    ent = (alarmas_entrada_t){
        .en_monitoreo = true, .en_pausa = false, .angulo_grados = 0.0f, .tolerancia_grados = 4.0f,
        .base_optica_ok = true, .bateria_pct = 100, .cargador = false,
    };
}

/* Empieza una infusión nueva con el reloj simulado donde estaba */
static void Nuevo(uint16_t fg, uint16_t vol_ml, uint16_t dur_min, const char *titulo)
{
    printf("\n== %s\n", titulo);
    Alarmas_Iniciar(fg, vol_ml, dur_min, t_us);
    prox_gota_us = -1;
    EntradaNormal();
}

/* Avanza seg segundos con gotas cada periodo_s (0 = sin gotas), evaluando las alarmas cada 1 s */
static void Simular(int seg, float periodo_s)
{
    int64_t per_us = (int64_t)(periodo_s * 1e6f);
    for (int s = 0; s < seg; s++) {
        int64_t fin = t_us + 1000000;
        if (per_us > 0) {
            if (prox_gota_us < t_us) {
                prox_gota_us = t_us + per_us;   // el goteo (re)empieza
            }
            while (prox_gota_us <= fin) {
                Alarmas_Gota(prox_gota_us);
                prox_gota_us += per_us;
            }
        }
        t_us = fin;
        Alarmas_Actualizar(&ent, t_us);
    }
}

/*==================[external functions definition]==========================*/
void app_main(void)
{
    Alarmas_SetCallback(OnEvento);
    printf("Test de alarmas con reloj simulado\n");

    /* 1. Resumen y T_ocl */
    printf("\n== 1. Validacion del Resumen y T_ocl\n");
    CHECK(Alarmas_ResumenValido(20, 500, 480), "500 mL en 8 h con FG20 (20,8 gotas/min) es valido");
    CHECK(!Alarmas_ResumenValido(10, 50, 1440), "50 mL en 24 h con FG10 (0,35 gotas/min) no se permite");
    Nuevo(20, 500, 480, "T_ocl 500 mL / 8 h / FG20");
    CHECK(Alarmas_TOclusionS() > 14.9f && Alarmas_TOclusionS() < 15.1f, "T_ocl = 15 s (el minimo)");
    Nuevo(10, 500, 1440, "T_ocl 500 mL / 24 h / FG10");
    CHECK(Alarmas_TOclusionS() > 51.0f && Alarmas_TOclusionS() < 53.0f, "T_ocl = 52 s (3 x T_obj)");

    /* 2. Goteo normal */
    Nuevo(20, 500, 480, "2. Goteo normal 5 min");
    Simular(300, T_OBJ_S);
    CHECK(Alarmas_Activas() == 0, "sin eventos con goteo normal");

    /* 3. Flujo detenido */
    Nuevo(20, 500, 480, "3. Flujo detenido");
    Simular(60, T_OBJ_S);
    Simular(10, 0);
    CHECK(!Activa(ALARMA_FLUJO_DETENIDO), "10 s sin gotas: todavia no (T_ocl = 15 s)");
    Simular(8, 0);
    CHECK(Activa(ALARMA_FLUJO_DETENIDO), "18 s sin gotas: FLUJO_DETENIDO");
    CHECK(!Activa(ALARMA_FIN_SACHET), "no es fin de sachet (volumen bajo)");
    Simular(12, T_OBJ_S);
    CHECK(!Activa(ALARMA_FLUJO_DETENIDO), "con 3 gotas seguidas se resuelve sola");

    /* 4. Desviacion */
    Nuevo(20, 500, 480, "4. Desviacion +40 %");
    Simular(180, T_OBJ_S);
    Simular(100, T_OBJ_S / 1.4f);
    CHECK(!Activa(ALARMA_DESVIACION_FUERTE), "100 s de desvio: la alarma fuerte espera 2 min");
    Simular(50, T_OBJ_S / 1.4f);
    CHECK(Activa(ALARMA_DESVIACION_FUERTE), "150 s de desvio: DESVIACION_FUERTE");
    CHECK(!Activa(AVISO_DESVIACION_LEVE), "el aviso leve espera 5 min");
    Simular(170, T_OBJ_S / 1.4f);
    CHECK(Activa(AVISO_DESVIACION_LEVE), "320 s de desvio: AVISO_DESVIACION_LEVE");
    CHECK(Alarmas_Desvio() > 0.35f && Alarmas_Desvio() < 0.45f, "desvio medido = +40 %");
    Simular(40, T_OBJ_S);
    CHECK(!Activa(ALARMA_DESVIACION_FUERTE) && !Activa(AVISO_DESVIACION_LEVE), "al volver al objetivo se apagan");

    Nuevo(20, 500, 480, "4b. Sin evaluar los primeros 2 min");
    Simular(100, T_OBJ_S / 1.4f);
    CHECK(Alarmas_Activas() == 0, "a los 100 s del Monitoreo no hay desvio ni flujo");

    /* 5. Colocacion */
    Nuevo(20, 500, 480, "5. Colocacion");
    Simular(30, T_OBJ_S);
    ent.angulo_grados = 10.0f;
    Simular(3, T_OBJ_S);
    CHECK(!Activa(ALARMA_COLOCACION), "3 s inclinado: todavia no");
    Simular(4, T_OBJ_S);
    CHECK(Activa(ALARMA_COLOCACION), "7 s inclinado: COLOCACION");
    CHECK(Alarmas_VolumenIncierto(), "el volumen queda marcado como incierto");
    ent.angulo_grados = 0.0f;
    Simular(3, T_OBJ_S);
    CHECK(Activa(ALARMA_COLOCACION), "3 s bien colocado: sigue activa");
    Simular(4, T_OBJ_S);
    CHECK(!Activa(ALARMA_COLOCACION), "7 s bien colocado: se resuelve sola");

    ent.base_optica_ok = false;
    Simular(7, T_OBJ_S);
    CHECK(Activa(ALARMA_COLOCACION), "base optica fuera de rango 7 s: COLOCACION");
    Simular(30, 0);
    CHECK(!Activa(ALARMA_FLUJO_DETENIDO), "con colocacion activa no hay FLUJO_DETENIDO");
    ent.base_optica_ok = true;
    Simular(7, T_OBJ_S);
    CHECK(!Activa(ALARMA_COLOCACION) && !Activa(ALARMA_FLUJO_DETENIDO), "al resolverse no queda ningun flujo detenido");

    /* 6. Bateria */
    Nuevo(20, 500, 480, "6. Bateria");
    Simular(1, T_OBJ_S);
    ent.bateria_pct = 25; Simular(1, T_OBJ_S);
    CHECK(!Activa(AVISO_BATERIA), "25 %: normal");
    ent.bateria_pct = 20; Simular(1, T_OBJ_S);
    CHECK(Activa(AVISO_BATERIA), "20 %: AVISO_BATERIA");
    ent.bateria_pct = 10; Simular(1, T_OBJ_S);
    CHECK(Activa(ALARMA_BATERIA_BAJA) && !Activa(AVISO_BATERIA), "10 %: BATERIA_BAJA (excluyente)");
    ent.bateria_pct = 5; Simular(1, T_OBJ_S);
    CHECK(Activa(ALARMA_BATERIA_CRITICA) && !Activa(ALARMA_BATERIA_BAJA), "5 %: BATERIA_CRITICA");
    ent.bateria_pct = 6; Simular(1, T_OBJ_S);
    CHECK(Activa(ALARMA_BATERIA_CRITICA), "6 %: sigue critica (histeresis)");
    ent.bateria_pct = 8; Simular(1, T_OBJ_S);
    CHECK(Activa(ALARMA_BATERIA_BAJA) && !Activa(ALARMA_BATERIA_CRITICA), "8 %: baja a BATERIA_BAJA");
    ent.cargador = true; Simular(1, T_OBJ_S);
    CHECK(!Activa(ALARMA_BATERIA_BAJA) && !Activa(AVISO_BATERIA), "con cargador se anulan");

    /* 7. Fin de infusion: 50 mL en 60 min, FG20 (16,7 gotas/min, 1000 gotas en total) */
    Nuevo(20, 50, 60, "7. Pre-aviso y fin de sachet (50 mL / 1 h)");
    Simular(3300, 3.6f);
    CHECK(Activa(AVISO_PROXIMO_A_FINALIZAR), "mas de 90 % del volumen: AVISO_PROXIMO_A_FINALIZAR");
    CHECK(!Activa(ALARMA_VOLUMEN_COMPLETO), "todavia no esta completo");
    CHECK(Alarmas_Activas() == ALARMA_BIT(AVISO_PROXIMO_A_FINALIZAR), "no hay otros eventos");
    Simular(40, 0);
    CHECK(Activa(ALARMA_FIN_SACHET) && !Activa(ALARMA_FLUJO_DETENIDO), "sin gotas con mas de 90 %: FIN_SACHET");

    Nuevo(20, 50, 60, "7b. Volumen completo con gotas");
    Simular(3700, 3.6f);
    CHECK(Activa(ALARMA_VOLUMEN_COMPLETO), "volumen programado superado y sigue goteando: VOLUMEN_COMPLETO");
    Simular(30, 0);
    CHECK(Activa(ALARMA_FIN_SACHET) && !Activa(ALARMA_VOLUMEN_COMPLETO), "al cortarse el goteo pasa a FIN_SACHET");

    /* 8. Prioridad, silenciado y re-armado */
    Nuevo(20, 500, 480, "8. Prioridad, silenciado y re-armado");
    Simular(30, T_OBJ_S);
    ent.bateria_pct = 4;
    ent.angulo_grados = 10.0f;
    Simular(8, T_OBJ_S);
    CHECK(Alarmas_Mostrar() == ALARMA_COLOCACION, "COLOCACION tiene prioridad sobre BATERIA_CRITICA");
    Alarmas_Silenciar(t_us);
    CHECK(Alarmas_Mostrar() == ALARMA_BATERIA_CRITICA, "silenciada la colocacion aparece la bateria critica");
    CHECK(Alarmas_Pie() == ALARMA_COLOCACION, "el pie muestra la silenciada");
    Alarmas_Silenciar(t_us);
    CHECK(Alarmas_Mostrar() == -1, "silenciadas las dos no queda ninguna");
    Simular(100, T_OBJ_S);
    CHECK(Alarmas_Mostrar() == -1, "a los 100 s siguen silenciadas");
    Simular(25, T_OBJ_S);
    CHECK(Alarmas_Mostrar() == ALARMA_COLOCACION, "a los 2 min se re-arman (alta)");

    printf("\n==== Resumen: %d PASS, %d FALLA ====\n", pass, fail);
    printf("FIN\n");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*==================[end of file]============================================*/
