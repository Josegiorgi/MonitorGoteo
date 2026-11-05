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
 * @note La primera vez que corre con la partición (o con una flash sin formatear) la borra entera: tarda
 * unos 10 s y pasa una sola vez.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 05/11/2026 | Document creation                              |
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
    REG_REINICIO,      /*!< El equipo se reinició con una infusión activa y la retomó: a = 1 si el autotest falló */
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
    uint32_t gotas;     /*!< Muestras: contador de gotas */
} registro_linea_t;

/**
 * @brief Infusión activa guardada en la NVS para poder retomarla después de un reinicio
 */
typedef struct {
    uint16_t factor_goteo;    /*!< gtt/mL */
    uint16_t volumen_ml;      /*!< Volumen programado, en mL */
    uint16_t duracion_min;    /*!< Duración programada, en minutos */
    uint32_t inicio_linea;    /*!< Número de la línea del registro donde empezó esta infusión (REG_INICIO) */
} registro_infusion_t;

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

/** @fn const char *Registro_NombreEvento(uint8_t evento)
 * @brief Nombre del evento, para la consola y la descarga
 */
const char *Registro_NombreEvento(uint8_t evento);

/** @} */
#endif /* REGISTRO_H */
