#ifndef ALARMAS_H
#define ALARMAS_H
/** \addtogroup Alarmas Alarmas
 ** @{ */

/** \brief Lógica de avisos y alarmas de la infusión, según la especificación de alarmas.
 *
 * Mantiene un bitmask de eventos activos y otro de silenciados. No usa hardware ni relojes
 * propios: todas las funciones reciben la hora actual en microsegundos (por ejemplo
 * esp_timer_get_time()), igual que goteo.c, así se puede probar con un reloj simulado.
 *
 * Eventos, de mayor a menor prioridad (el número del enum es la prioridad: 0 es la mayor):
 *
 * | Evento                    | Tipo         | Se activa cuando                                         |
 * |:--------------------------|:------------:|:---------------------------------------------------------|
 * | VOLUMEN_COMPLETO          | alta         | V infundido >= V programado y siguen cayendo gotas       |
 * | FLUJO_DETENIDO            | alta         | sin gotas más de T_ocl y V infundido < 90 %              |
 * | FIN_SACHET                | alta         | sin gotas más de T_ocl y V infundido >= 90 %             |
 * | COLOCACION                | alta         | inclinación o base óptica fuera de rango, 5 s seguidos   |
 * | BATERIA_CRITICA           | alta         | batería <= 5 %                                           |
 * | DESVIACION_FUERTE         | media        | desvío del caudal > 25 % durante 2 min                   |
 * | BATERIA_BAJA              | media        | batería <= 10 %                                          |
 * | AVISO_DESVIACION_LEVE     | aviso        | desvío del caudal > 10 % durante 5 min                   |
 * | AVISO_PROXIMO_A_FINALIZAR | aviso        | V infundido >= 90 % del programado                       |
 * | AVISO_BATERIA             | aviso        | batería <= 20 %                                          |
 *
 * Los siete primeros suenan (mismo orden que buzzer_alarm_t); los avisos no suenan. FLUJO_DETENIDO
 * y FIN_SACHET son excluyentes. Los tres escalones de batería también (queda solo el peor).
 *
 * @note Los parámetros (T_ocl, umbrales, persistencias) están como macros en alarmas.c. Los
 * valores salen de la especificación; los que dice que se ajustan con ensayos de banco (90 % de
 * fin de sachet, rango óptico, tolerancia de ángulo) son provisorios.
 *
 * @note El caudal medido se calcula en float con la mediana de los últimos 10 intervalos entre
 * gotas, no con los mL/h enteros que se muestran en pantalla.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>

/*==================[typedef]================================================*/
/**
 * @brief Eventos (el valor es el orden de prioridad: 0 = la mayor)
 */
typedef enum {
    ALARMA_VOLUMEN_COMPLETO = 0,
    ALARMA_FLUJO_DETENIDO,
    ALARMA_FIN_SACHET,
    ALARMA_COLOCACION,
    ALARMA_BATERIA_CRITICA,
    ALARMA_DESVIACION_FUERTE,
    ALARMA_BATERIA_BAJA,
    AVISO_DESVIACION_LEVE,
    AVISO_PROXIMO_A_FINALIZAR,
    AVISO_BATERIA,
    ALARMAS_CANTIDAD,
} alarma_evento_t;

#define ALARMA_BIT(e)           (1u << (e))
#define ALARMAS_SONORAS_MASK    (ALARMA_BIT(AVISO_DESVIACION_LEVE) - 1u)   /*!< Eventos con sonido (alarmas altas y medias) */
#define ALARMAS_ALTAS_MASK      (ALARMA_BIT(ALARMA_DESVIACION_FUERTE) - 1u) /*!< Alarmas de prioridad alta (se re-arman a los 2 min) */

/**
 * @brief Lo que el main le informa a las alarmas en cada actualización
 */
typedef struct {
    bool  en_monitoreo;        /*!< true desde que empieza el Monitoreo hasta que se finaliza la infusión (incluye las pantallas de alarma) */
    bool  en_pausa;            /*!< true si la infusión está pausada */
    float angulo_grados;       /*!< Ángulo del equipo respecto de la vertical (ADXL345, filtrado) */
    float tolerancia_grados;   /*!< Rango correcto del ángulo (+/- este valor) */
    bool  base_optica_ok;      /*!< true si el nivel de base del fotodiodo está en el rango normal (equipo en la cámara) */
    uint8_t bateria_pct;       /*!< Carga de la batería, 0 a 100 */
    bool  cargador;            /*!< true con el cargador conectado: se anulan las alarmas de batería */
} alarmas_entrada_t;

/**
 * @brief Función que se llama cada vez que un evento se activa o se apaga (para registrarlo y
 * enviarlo por BLE con su hora)
 */
typedef void (*alarmas_cb_t)(alarma_evento_t evento, bool activo, int64_t ahora_us);

/*==================[external functions declaration]=========================*/

/** @fn bool Alarmas_ResumenValido(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min)
 * @brief true si se puede iniciar la infusión: el objetivo tiene que ser de al menos 2 gotas/min.
 * Con menos, T_ocl pasaría de lo permitido y el Resumen no debe habilitar "iniciar".
 */
bool Alarmas_ResumenValido(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min);

/** @fn void Alarmas_Iniciar(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min, int64_t ahora_us)
 * @brief Empieza una infusión: pone todo en cero (gotas, eventos, silenciados). Llamar al
 * empezar el Ajuste de goteo, que es cuando se empieza a contar el volumen.
 */
void Alarmas_Iniciar(uint16_t factor_goteo, uint16_t volumen_ml, uint16_t duracion_min, int64_t ahora_us);

/** @fn void Alarmas_Gota(int64_t ahora_us)
 * @brief Registra una gota detectada. Llamar una vez por cada gota.
 */
void Alarmas_Gota(int64_t ahora_us);

/** @fn void Alarmas_Actualizar(const alarmas_entrada_t *entrada, int64_t ahora_us)
 * @brief Evalúa todas las condiciones. Llamar cada ~1 s.
 */
void Alarmas_Actualizar(const alarmas_entrada_t *entrada, int64_t ahora_us);

/** @fn void Alarmas_SetCallback(alarmas_cb_t cb)
 * @brief Registra la función que se llama en cada activación o desactivación (NULL para quitarla)
 */
void Alarmas_SetCallback(alarmas_cb_t cb);

/** @fn void Alarmas_Silenciar(int64_t ahora_us)
 * @brief Silencia la alarma que se está mostrando (la de mayor prioridad sin silenciar). Si es de
 * prioridad alta se re-arma sola a los 2 min si la condición sigue activa.
 */
void Alarmas_Silenciar(int64_t ahora_us);

/** @fn uint32_t Alarmas_Activas(void)
 * @brief Bitmask de eventos activos (ALARMA_BIT)
 */
uint32_t Alarmas_Activas(void);

/** @fn uint32_t Alarmas_Silenciadas(void)
 * @brief Bitmask de alarmas silenciadas
 */
uint32_t Alarmas_Silenciadas(void);

/** @fn int Alarmas_Mostrar(void)
 * @brief Alarma con sonido de mayor prioridad que está activa y no silenciada (la que va en la
 * pantalla de alarma y suena), o -1 si no hay.
 */
int Alarmas_Mostrar(void);

/** @fn int Alarmas_Pie(void)
 * @brief Aviso o alarma silenciada de mayor prioridad (el que va en el pie del Monitoreo), o -1.
 */
int Alarmas_Pie(void);

/** @fn float Alarmas_Desvio(void)
 * @brief Último desvío del caudal respecto del objetivo (+0.25 = 25 % más rápido). 0 si todavía
 * no se pudo calcular.
 */
float Alarmas_Desvio(void);

/** @fn uint16_t Alarmas_VolumenMl(void)
 * @brief Volumen infundido estimado en mL (gotas / factor de goteo)
 */
uint16_t Alarmas_VolumenMl(void);

/** @fn bool Alarmas_VolumenIncierto(void)
 * @brief true si hubo una alarma de colocación durante la infusión (el volumen estimado puede no
 * contar gotas que pasaron con el equipo desacomodado)
 */
bool Alarmas_VolumenIncierto(void);

/** @fn float Alarmas_TOclusionS(void)
 * @brief T_ocl calculado para la infusión actual, en segundos
 */
float Alarmas_TOclusionS(void);

/** @fn const char *Alarmas_Nombre(alarma_evento_t evento)
 * @brief Nombre corto del evento, para mensajes de depuración
 */
const char *Alarmas_Nombre(alarma_evento_t evento);

/** @fn void AlarmasSonido_Actualizar(void)
 * @brief Hace sonar en el buzzer la alarma de Alarmas_Mostrar() (o lo silencia si no hay).
 * Llamar después de cada Alarmas_Actualizar() y de cada Alarmas_Silenciar(). Requiere
 * BuzzerAlarm_Init().
 */
void AlarmasSonido_Actualizar(void);

/** @} */
#endif /* ALARMAS_H */
