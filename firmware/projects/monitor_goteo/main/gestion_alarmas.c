/*==================[inclusions]=============================================*/
#include "gestion_alarmas.h"
#include "estado.h"
#include "infusion.h"
#include "pendiente.h"
#include "alarmas.h"
#include "inclinacion.h"
#include "bateria.h"
#include <math.h>

/*==================[internal data definition]===============================*/
static bool angulo_valido = false;     /*!< true si la última lectura del acelerómetro es confiable */

/*==================[internal functions definition]==========================*/
/** Alarma de la interfaz que corresponde a un evento del componente alarmas */
static ui_alarma_t AlarmaDeInterfaz(int evento) {
    switch (evento) {
    case ALARMA_FLUJO_DETENIDO:     return UI_ALARMA_OCLUSION;
    case ALARMA_DESVIACION_FUERTE:  return UI_ALARMA_DESVIACION_FLUJO;
    case ALARMA_FIN_SACHET:
    case ALARMA_VOLUMEN_COMPLETO:   return UI_ALARMA_FIN_SACHET;   // la interfaz todavía no tiene pantalla propia para volumen completo
    case ALARMA_BATERIA_CRITICA:    return UI_ALARMA_BATERIA_CRITICA;
    case ALARMA_BATERIA_BAJA:       return UI_ALARMA_BATERIA_BAJA;
    case ALARMA_COLOCACION:         return UI_ALARMA_COLOCACION;
    case ALARMA_EQUIPO_REINICIADO:  return UI_ALARMA_EQUIPO_REINICIADO;
    case ALARMA_GOTEO_SIN_MONITOREO: return UI_ALARMA_GOTEO_SIN_MONITOREO;
    default:                        return UI_ALARMA_NINGUNA;
    }
}

/** Pasa a la interfaz la alarma que corresponde mostrar: la de mayor prioridad sin silenciar o,
 * si no hay, la silenciada de mayor prioridad (queda el aviso abajo en el Monitoreo). */
static void CargarAlarmaEnInterfaz(void) {
    datos.aviso_bateria = (Alarmas_Activas() & ALARMA_BIT(AVISO_BATERIA)) != 0;
    datos.desvio_pct = (int16_t)lroundf(Alarmas_Desvio() * 100.0f);   // la alarma de desvío lo muestra
    int mostrar = Alarmas_Mostrar();
    int pie = Alarmas_Pie();
    if (mostrar >= 0) {
        datos.alarma = AlarmaDeInterfaz(mostrar);
        datos.alarma_silenciada = false;
    } else if (pie >= 0 && pie < AVISO_DESVIACION_LEVE) {
        datos.alarma = AlarmaDeInterfaz(pie);
        datos.alarma_silenciada = true;
    } else {
        datos.alarma = UI_ALARMA_NINGUNA;
        datos.alarma_silenciada = false;
    }
}

/** true si la infusión está en curso y hay que evaluar las alarmas (desde el Monitoreo hasta el fin) */
static bool EnMonitoreo(void) {
    if (!midiendo || !datos.infusion_configurada) {
        return false;
    }
    if (UI_EditandoDatos()) {
        return true;   // cambiando la duración desde Ajustes: el monitoreo sigue
    }
    switch (UI_PantallaActual()) {
    case UI_PANTALLA_MONITOREO:
    case UI_PANTALLA_ALARMA:
    case UI_PANTALLA_ACCESO:
    case UI_PANTALLA_AJUSTES:
    case UI_PANTALLA_DETENER:
    case UI_PANTALLA_APAGAR:
    case UI_PANTALLA_COLOCACION:
        return true;
    default:
        return false;
    }
}

/*==================[external functions definition]==========================*/
void GestionAlarmas_ActualizarAngulo(void) {
    float grados = 0.0f;
    angulo_valido = Inclinacion_Valida(&grados);
    datos.angulo_grados = angulo_valido ? grados : 0.0f;
    datos.colocacion_ok = angulo_valido && fabsf(grados) <= datos.tolerancia_grados;
}

void GestionAlarmas_ActualizarBateria(void) {
    datos.bateria_pct = Bateria_Valida() ? Bateria_Pct() : 100;
}

void GestionAlarmas_Evaluar(int64_t ahora, bool forzar) {
    bool sin_infusion = Infusion_SinInfusionEvaluable() || (forzar && !midiendo);
    if (!midiendo && !sin_infusion) {
        // Pantallas donde no se vigila (Colocación, Config, Resumen, autotest): si quedó una alarma activa o sonando se apaga,
        // para que no siga sonando ni tape los sonidos de la interfaz (por ejemplo el beep del recordatorio del Ajuste)
        if (Alarmas_Activas() != 0) {
            Alarmas_Iniciar(datos.factor_goteo, datos.volumen_total_ml, datos.duracion_min, ahora);
            CargarAlarmaEnInterfaz();
            AlarmasSonido_Actualizar();
        }
        return;
    }
    alarmas_entrada_t entrada = {
        .en_monitoreo = EnMonitoreo(),
        .en_pausa = datos.en_pausa,
        .angulo_grados = angulo_valido ? datos.angulo_grados : 2.0f * datos.tolerancia_grados,   // sin lectura confiable, fuera de la tolerancia
        .tolerancia_grados = datos.tolerancia_grados,
        .base_optica_ok = true,          // TODO: nivel de base del fotodiodo en rango
        .bateria_pct = datos.bateria_pct,
        .cargador = false,               // sin detección de cargador (decidido): el aumento de tensión al cargar resuelve la alarma
        .equipo_reiniciado = Pendiente_EquipoReiniciado(),
        .sin_infusion = sin_infusion,
    };
    Alarmas_Actualizar(&entrada, ahora);
    CargarAlarmaEnInterfaz();
    AlarmasSonido_Actualizar();
}

void GestionAlarmas_Silenciar(int64_t ahora) {
    int silenciada = Alarmas_Mostrar();
    if (silenciada >= 0) {
        Infusion_AnotarSilenciada(silenciada, ahora);
    }
    Alarmas_Silenciar(ahora);
    CargarAlarmaEnInterfaz();
    AlarmasSonido_Actualizar();
    UI_Actualizar(&datos);
}
