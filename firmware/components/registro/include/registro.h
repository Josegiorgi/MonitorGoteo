#ifndef REGISTRO_H
#define REGISTRO_H
/** \addtogroup Registro Registro de la infusión
 ** @{ */

/** \brief Registro de la infusión en una partición propia de la flash, y estado de la infusión activa en la NVS.
 *
 * Según la sección 5.1 de la especificación de inicio y navegación:
 *
 * - **Registro de la infusión:** todo lo que pasa desde el INICIO hasta el FIN, guardado en la partición
 *   "registro" de la flash (tipo data, subtipo 0x40). Es una sucesión de líneas binarias de tamaño fijo
 *   (REGISTRO_LINEA_B = 16 bytes) que se escriben una detrás de otra, en el momento en que ocurren: un
 *   reinicio pierde como mucho la línea en curso. Hay dos tipos de línea:
 *   eventos (INICIO, AJUSTE_OK, CAMBIO_DATOS, claves, avisos y alarmas, REINICIO, DESCARGA y FIN) y
 *   muestras (cada 1 min: contador de gotas, caudal x10 y volumen x10). El tamaño fijo permite que
 *   la descarga desde la línea N salte directo a la posición N x 16.
 * - **Nunca se borra solo:** si no hay lugar para la duración programada, el Resumen no deja iniciar (Registro_MinutosLibres).
 *   El registro se borra desde el Modo Técnico.
 * - **NVS:** solo la configuración de la infusión activa y dónde empieza en el registro (se escriben al
 *   INICIO, en CAMBIO_DATOS y se borran al FIN) y la marca de registro descargado. El minuto y el contador
 *   de gotas para retomar tras un reinicio se recuperan de la última muestra del registro.
 *
 * Como el equipo no tiene RTC, el tiempo que estuvo apagado no se conoce: al retomar, el minuto y el
 * contador continúan desde la última muestra.
 *
 * @section guardado Cuándo se guarda qué
 *
 * | Momento                                          | Qué se guarda                                           | Dónde          |
 * |:-------------------------------------------------|:--------------------------------------------------------|:---------------|
 * | Se confirma el Resumen (entra al Ajuste de goteo) | Configuración y línea del registro donde empieza        | NVS            |
 * | El mismo momento                                 | REG_INICIO y REG_DURACION                               | Registro       |
 * | Se confirma el Ajuste de goteo                   | REG_AJUSTE_OK                                           | Registro       |
 * | Cada 1 min, desde que empieza el Monitoreo      | Muestra: minuto, gotas, caudal x10 y volumen x10        | Registro       |
 * | Ajustes > Cambiar datos                          | REG_CAMBIO_DATOS y la nueva configuración               | Registro y NVS |
 * | Un aviso o alarma se activa o se resuelve        | REG_ALARMA (activo = 1 o 0)                             | Registro       |
 * | Se silencia una alarma                           | REG_SILENCIADO                                          | Registro       |
 * | Se ingresa una clave                             | REG_CLAVE_OK, REG_CLAVE_ERROR o REG_TECNICO_OK          | Registro       |
 * | El equipo retoma tras un reinicio                | REG_REINICIO                                            | Registro       |
 * | Detener monitoreo o Apagar con infusión activa   | REG_FIN (volumen infundido) y se borra la infusión activa | Registro y NVS |
 *
 * No se guarda: lo que se edita en Config antes de confirmar el Resumen, cada gota por separado (solo el
 * contador de cada minuto), el contador entre una muestra y la siguiente (un corte pierde como mucho 1 min),
 * la navegación entre pantallas, el resultado del autotest, la batería en el tiempo (solo los avisos y alarmas
 * de batería) y las acciones del Modo Técnico salvo el ingreso. Los eventos de claves y alarmas se anotan aunque
 * no haya una infusión activa (con minuto 0).
 *
 * Si el equipo se corta antes de la primera muestra, retoma con el minuto y las gotas en 0.
 *
 * @note La primera vez que corre con la partición (o con una flash sin formatear) la borra entera: tarda
 * unos 10 s y pasa una sola vez.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 05/11/2026 | Document creation                              |
 * | 10/11/2026 | Estado de la última infusión y registro pendiente de descarga (Registro_LeerPendiente / Registro_EscribirPendiente): la NVS guarda si terminó, si fue interrumpida por un reinicio y si espera la descarga. Ya no se retoma tras un reinicio |
 * | 07/11/2026 | Registro_ActivarLog y Registro_Volcar: ver por consola lo que se guarda, para probar sin BLE |
 * | 07/11/2026 | Para el BLE: ID de infusión (Registro_NuevoId), última infusión (Registro_UltimaInfusion) y un dato extra de 32 bits en los eventos (Registro_AnotarConDato) |
 * | 05/11/2026 | Registro en una partición propia de la flash (líneas de 16 B, eventos y muestras por minuto) en lugar de la NVS, según la especificación actualizada |
 *
 */

#include <stdint.h>
#include <stdbool.h>

/*==================[macros]=================================================*/
#define REGISTRO_LINEA_B   16   /*!< Tamaño de una línea del registro, en bytes */

/*==================[typedef]================================================*/
/**
 * @brief Eventos del registro
 */
typedef enum {
    REG_INICIO = 1,    /*!< Se abre la infusión: a = factor de goteo, b = volumen (mL) */
    REG_DURACION,      /*!< Sigue a REG_INICIO: a = duración programada (min) */
    REG_AJUSTE_OK,     /*!< La enfermera confirmó el Ajuste de goteo */
    REG_CAMBIO_DATOS,  /*!< Ajustes > Cambiar datos: a = duración anterior, b = duración nueva (min) */
    REG_FIN,           /*!< Fin de la infusión (detenida o apagada): a = volumen infundido (mL) */
    REG_REINICIO,      /*!< El equipo se reinició con una infusión sin terminar (queda interrumpida) o con un registro sin descargar; el minuto es el último minuto guardado */
    REG_CLAVE_OK,      /*!< Clave clínica correcta */
    REG_CLAVE_ERROR,   /*!< Clave incorrecta */
    REG_TECNICO_OK,    /*!< Se ingresó el código técnico */
    REG_ALARMA,        /*!< Aviso o alarma: activo = 1 al activarse, 0 al resolverse; a = evento de alarmas (alarma_evento_t) */
    REG_SILENCIADO,    /*!< El cuidador silenció una alarma: a = evento de alarmas */
    REG_DESCARGA,      /*!< La app confirmó la recepción del registro por BLE: a = última línea recibida (parte baja) */
} registro_evento_t;

/**
 * @brief Tipo de línea
 */
typedef enum {
    REG_TIPO_EVENTO = 1,   /*!< Evento: usa evento, activo, a y b */
    REG_TIPO_MUESTRA = 2,  /*!< Muestra de cada 1 min: usa gotas, a (caudal x10, mL/h) y b (volumen x10, mL) */
} registro_tipo_t;

/**
 * @brief Una línea del registro: exactamente lo que se guarda en la flash (16 bytes)
 */
typedef struct __attribute__((packed)) {
    uint32_t minuto;    /*!< Minutos desde el inicio de la infusión (0 si no hay una activa) */
    uint8_t  tipo;      /*!< registro_tipo_t (0xFF = línea sin escribir) */
    uint8_t  evento;    /*!< registro_evento_t (solo eventos) */
    uint8_t  activo;    /*!< 1 = se activó, 0 = se resolvió (solo REG_ALARMA) */
    uint8_t  reservado; /*!< Sin uso */
    uint16_t a;         /*!< Eventos: primer dato. Muestras: caudal x10 */
    uint16_t b;         /*!< Eventos: segundo dato. Muestras: volumen x10 */
    uint32_t gotas;     /*!< Muestras: contador de gotas. Eventos: dato extra (REG_INICIO: ID de la infusión, REG_FIN: gotas totales) */
} registro_linea_t;

/**
 * @brief Infusión activa guardada en la NVS para poder retomarla después de un reinicio
 */
typedef struct {
    uint16_t factor_goteo;    /*!< gtt/mL */
    uint16_t volumen_ml;      /*!< Volumen programado, en mL */
    uint16_t duracion_min;    /*!< Duración programada, en minutos */
    uint32_t inicio_linea;    /*!< Número de la línea del registro donde empezó esta infusión (REG_INICIO) */
    uint32_t id;              /*!< ID de la infusión: único por infusión, no cambia con un reinicio ni con Cambiar datos */
} registro_infusion_t;

/** Estados de la última infusión (los mismos valores que informa ESTADO por BLE) */
#define REGISTRO_ESTADO_TERMINADA     0   /*!< Terminó (Detener monitoreo o Apagar): tiene línea FIN */
#define REGISTRO_ESTADO_EN_CURSO      1   /*!< Sigue en curso */
#define REGISTRO_ESTADO_INTERRUMPIDA  2   /*!< El equipo se reinició antes de que terminara: no tiene línea FIN, termina en REINICIO */

/**
 * @brief Estado de la última infusión y de su descarga, guardado en la NVS junto con su ID y su línea de inicio
 */
typedef struct {
    uint8_t  estado;            /*!< REGISTRO_ESTADO_* */
    bool     pendiente;         /*!< true si todavía no se recibió el OK de la app (el equipo sigue contando minutos) */
    uint32_t base_minuto;       /*!< Interrumpida: minuto del último REINICIO (los minutos que se informan se cuentan desde ahí) */
    uint32_t minuto_congelado;  /*!< Minuto informado cuando se recibió el OK (ya no se cuenta más) */
} registro_pendiente_t;

/*==================[external functions declaration]=========================*/

/** @fn bool Registro_Init(void)
 * @brief Inicia la NVS, encuentra la partición del registro (y la formatea la primera vez) y busca dónde
 * termina lo escrito. Se puede llamar más de una vez.
 * @return true si todo quedó listo
 */
bool Registro_Init(void);

/** @fn bool Registro_Probar(void)
 * @brief Chequeo "Memoria" del autotest: escribe y lee la NVS y lee la partición del registro
 */
bool Registro_Probar(void);

/** @fn void Registro_Anotar(registro_evento_t evento, uint8_t activo, uint32_t minuto, uint16_t a, uint16_t b)
 * @brief Agrega un evento al registro. Pone el registro como "no descargado".
 */
void Registro_Anotar(registro_evento_t evento, uint8_t activo, uint32_t minuto, uint16_t a, uint16_t b);

/** @fn void Registro_AnotarConDato(registro_evento_t evento, uint8_t activo, uint32_t minuto, uint16_t a, uint16_t b, uint32_t dato)
 * @brief Igual que Registro_Anotar, con un dato extra de 32 bits (REG_INICIO: ID de la infusión; REG_FIN: gotas totales)
 */
void Registro_AnotarConDato(registro_evento_t evento, uint8_t activo, uint32_t minuto, uint16_t a, uint16_t b, uint32_t dato);

/** @fn uint32_t Registro_NuevoId(void)
 * @brief ID para una infusión nueva: el contador sube de a uno y queda guardado en la NVS. No se reinicia al borrar el registro.
 */
uint32_t Registro_NuevoId(void);

/** @fn bool Registro_UltimaInfusion(uint32_t *id, uint32_t *inicio_linea)
 * @brief ID y línea de inicio de la última infusión que se abrió (la activa o, si ya terminó, la que terminó). Es lo que
 * descarga la app. Se guarda junto con la infusión activa y no se borra al finalizar.
 * @return true si hay una infusión en el registro
 */
bool Registro_UltimaInfusion(uint32_t *id, uint32_t *inicio_linea);

/** @fn bool Registro_LeerPendiente(registro_pendiente_t *pendiente)
 * @brief Estado de la última infusión y de su descarga
 * @return false si todavía no hubo ninguna infusión
 */
bool Registro_LeerPendiente(registro_pendiente_t *pendiente);

/** @fn bool Registro_EscribirPendiente(const registro_pendiente_t *pendiente)
 * @brief Guarda el estado de la última infusión y de su descarga (al FIN, al reiniciar y al recibir el OK)
 */
bool Registro_EscribirPendiente(const registro_pendiente_t *pendiente);

/** @fn void Registro_Muestra(uint32_t minuto, uint32_t gotas, uint16_t caudal_x10, uint16_t volumen_x10)
 * @brief Agrega la muestra de cada 1 min: contador de gotas, caudal (mL/h x10) y volumen (mL x10)
 */
void Registro_Muestra(uint32_t minuto, uint32_t gotas, uint16_t caudal_x10, uint16_t volumen_x10);

/** @fn uint32_t Registro_Total(void)
 * @brief Líneas escritas en el registro (también es el número de la próxima línea)
 */
uint32_t Registro_Total(void);

/** @fn bool Registro_Leer(uint32_t indice, registro_linea_t *linea)
 * @brief Lee la línea número "indice" (la 0 es la primera)
 */
bool Registro_Leer(uint32_t indice, registro_linea_t *linea);

/** @fn bool Registro_UltimaMuestra(uint32_t desde_linea, uint32_t *minuto, uint32_t *gotas)
 * @brief Busca la última muestra escrita desde la línea indicada (para retomar la infusión tras un reinicio)
 * @return true si había alguna
 */
bool Registro_UltimaMuestra(uint32_t desde_linea, uint32_t *minuto, uint32_t *gotas);

/** @fn uint16_t Registro_MinutosLibres(void)
 * @brief Minutos de infusión para los que todavía alcanza el lugar del registro (una muestra por minuto
 * más un margen para los eventos). El Resumen no deja iniciar una duración mayor.
 */
uint16_t Registro_MinutosLibres(void);

/** @fn bool Registro_Borrar(void)
 * @brief Borra todo el registro (Modo Técnico). Tarda ~10 s. Borra también la infusión activa guardada.
 */
bool Registro_Borrar(void);

/** @fn bool Registro_Descargado(void)
 * @brief true si la app ya descargó el registro (desde la última línea anotada)
 */
bool Registro_Descargado(void);

/** @fn void Registro_MarcarDescargado(void)
 * @brief La app confirmó la descarga del registro con FIN por BLE
 */
void Registro_MarcarDescargado(void);

/** @fn bool Registro_GuardarInfusion(const registro_infusion_t *infusion)
 * @brief Guarda la infusión activa en la NVS (al INICIO y en CAMBIO_DATOS)
 */
bool Registro_GuardarInfusion(const registro_infusion_t *infusion);

/** @fn bool Registro_CargarInfusion(registro_infusion_t *infusion)
 * @brief Lee la infusión activa guardada
 * @return true si hay una infusión activa guardada
 */
bool Registro_CargarInfusion(registro_infusion_t *infusion);

/** @fn void Registro_BorrarInfusion(void)
 * @brief Borra la infusión activa de la NVS (al FIN)
 */
void Registro_BorrarInfusion(void);

/** @fn void Registro_ActivarLog(bool activar)
 * @brief Con activar = true cada línea que se escribe en el registro también sale por la consola (para probar la memoria
 * sin el BLE). Apagado por defecto.
 */
void Registro_ActivarLog(bool activar);

/** @fn void Registro_Volcar(uint32_t max_lineas)
 * @brief Imprime por la consola las últimas max_lineas líneas del registro. Sirve para comprobar que lo guardado
 * sobrevive a un reinicio.
 */
void Registro_Volcar(uint32_t max_lineas);

/** @fn const char *Registro_NombreEvento(uint8_t evento)
 * @brief Nombre del evento, para la consola y la descarga
 */
const char *Registro_NombreEvento(uint8_t evento);

/** @} */
#endif /* REGISTRO_H */
