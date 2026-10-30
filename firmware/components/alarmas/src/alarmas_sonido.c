/*==================[inclusions]=============================================*/
#include "alarmas.h"
#include "buzzer_alarm.h"

/*==================[macros and definitions]=================================*/
/* Los patrones del buzzer están en el mismo orden que los eventos con sonido */
_Static_assert((int)ALARMA_VOLUMEN_COMPLETO == (int)BUZZER_ALARM_VOLUMEN_COMPLETO, "orden distinto");
_Static_assert((int)ALARMA_FLUJO_DETENIDO == (int)BUZZER_ALARM_FLUJO_DETENIDO, "orden distinto");
_Static_assert((int)ALARMA_FIN_SACHET == (int)BUZZER_ALARM_FIN_SACHET, "orden distinto");
_Static_assert((int)ALARMA_COLOCACION == (int)BUZZER_ALARM_COLOCACION, "orden distinto");
_Static_assert((int)ALARMA_BATERIA_CRITICA == (int)BUZZER_ALARM_BATERIA_CRITICA, "orden distinto");
_Static_assert((int)ALARMA_DESVIACION_FUERTE == (int)BUZZER_ALARM_DESVIACION_FUERTE, "orden distinto");
_Static_assert((int)ALARMA_BATERIA_BAJA == (int)BUZZER_ALARM_BATERIA_BAJA, "orden distinto");
_Static_assert((int)AVISO_DESVIACION_LEVE == (int)BUZZER_ALARM_CANTIDAD, "los avisos no suenan");

/*==================[internal data definition]===============================*/
static int sonando = -1;   /*!< Alarma que se mandó al buzzer (-1 = ninguna) */

/*==================[external functions definition]==========================*/
void AlarmasSonido_Actualizar(void) {
    int deberia = Alarmas_Mostrar();
    if (deberia == sonando) {
        return;
    }
    sonando = deberia;
    if (deberia < 0) {
        BuzzerAlarm_Stop();
    } else {
        BuzzerAlarm_Start((buzzer_alarm_t)deberia);
    }
}
