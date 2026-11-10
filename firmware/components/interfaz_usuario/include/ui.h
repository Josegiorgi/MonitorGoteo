#ifndef UI_H
#define UI_H
/** \addtogroup Interfaz_Usuario Interfaz de usuario
 ** @{ */

/** \brief Pantallas de la OLED (u8g2, SSD1315 128x64 por I2C) y navegación con el encoder.
 *
 * El módulo muestra los datos que le carga el main (ui_datos_t) y maneja la navegación entre
 * pantallas con los eventos del encoder: giro = mover o cambiar, click = "Pulsar",
 * click largo = "Mantener" y doble click (solo en Monitoreo, alarma y fin de infusión). No mide nada ni decide las alarmas: el main (con el módulo de
 * sensado) completa ui_datos_t, incluida la alarma activa, y la interfaz la muestra.
 *
 * Flujo de pantallas:
 *
 * Inicio (el autotest se hace ahí; si un chequeo falla se muestra Autotest) -> Clave -> Colocación ->
 * Config 1/3..3/3 -> Resumen -> Ajuste de goteo -> Monitoreo. La clave clínica (4 dígitos) se pide en
 * el arranque y para entrar a Ajustes: el cuidador solo puede silenciar alarmas, así que mantener el
 * botón 3 s en el Monitoreo abre la clave y de ahí Ajustes (Cambiar datos, Detener monitoreo, Apagar
 * y Volver). Cambiar datos edita solo la duración y pasa por el Resumen y el Ajuste de goteo sin
 * reiniciar el contador. Detener monitoreo (con confirmación) deja "Sin infusión activa": con la
 * sesión abierta se puede elegir Nueva infusión o Apagar y a los 60 s sin actividad se bloquea (mantener
 * 3 s + clave). Una alarma interrumpe el Monitoreo con su pantalla y el click solo la silencia (también
 * la de fin de sachet). La de colocación vuelve a la pantalla de Colocación y, durante la infusión, se
 * resuelve sola cuando el ángulo vuelve al rango.
 *
 * @note I2C: SCL = GPIO_7, SDA = GPIO_21.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 15/10/2026 | Document creation                              |
 * | 04/11/2026 | El click en la alarma de fin de sachet solo silencia (antes daba por terminada la infusión); nuevo ítem "Finalizar infusión" en el Menú técnico; la pantalla de Colocación durante la infusión se resuelve sola |
 * | 05/11/2026 | El Resumen no deja iniciar si no queda lugar en la memoria del registro (registro_minutos_libres): "Memoria llena. Llamar a soporte técnico" |
 * | 05/11/2026 | UI_SetTiempos: acorta los tiempos de la interfaz para probar |
 * | 05/11/2026 | Modo Técnico (código técnico en la clave del arranque): Diagnóstico, Probar alarma, Borrar registro y Salir; a los 5 min sin actividad sale solo. UI_PantallaMensaje |
 * | 10/11/2026 | Registro pendiente de descarga: aviso en "Sin infusión activa" (sin sonido), confirmación "Hay un registro sin descargar. ¿Continuar igual?" antes de una infusión nueva y al apagar. Alarma EQUIPO_REINICIADO en lugar de FALLA_EQUIPO; se quitan el volumen incierto y "Monitoreo reanudado" (ya no se retoma). El código técnico también se acepta en la clave de "Sin infusión activa" |
 * | 05/11/2026 | Alarmas FALLA_EQUIPO y GOTEO_SIN_MONITOREO (esta última también se muestra en Sin infusión y en la clave del arranque); volumen incierto (">") y "Monitoreo reanudado" tras un reinicio; UI_SetCallbackClave para el registro |
 * | 05/11/2026 | Autotest con cinco chequeos que muestra "Falla: <nombre>" y qué hacer si algo falla (ui_chequeo_t.accion); el Resumen tiene dos páginas (factor de goteo destacado con "Verificar en el envase de la guía", y volumen, duración, mL/h y gtt/min); recordatorio_ajuste en el Ajuste de goteo |
 * | 05/11/2026 | Menú de Ajustes de la especificación (Cambiar datos, Detener monitoreo, Apagar, Volver), con confirmación de Detener y de Apagar y pantalla Sin infusión activa (Nueva infusión o Apagar; se bloquea a los 60 s). Cambiar datos edita solo la duración y pasa por el Resumen y el Ajuste de goteo sin reiniciar el contador. El Resumen no deja iniciar con menos de 2 gtt/min |
 * | 05/11/2026 | La alarma de desvío de flujo muestra el % de desvío (arriba a la derecha) y las gotas/min del objetivo (nuevo campo desvio_pct) |
 * | 05/11/2026 | Clave clínica de 4 dígitos (UI_CLAVE_CLINICA) después del autotest y para entrar a Ajustes: bloqueo de 60 s tras 3 errores, vuelve sola al Monitoreo a los 60 s sin actividad, una alarma nueva la interrumpe. Pulsación larga de 3 s (ENC_CLICK_MUY_LARGO) en Monitoreo y Fin de infusión reemplaza al doble click. El autotest ya no tiene pantalla propia salvo que falle un chequeo |
 * | 05/11/2026 | Durante la infusión, el click en la pantalla de Colocación silencia la alarma y vuelve al Monitoreo (queda "! COLOCACION" abajo); se resuelve sola con el ángulo en rango |
 * | 05/11/2026 | Nuevo campo aviso_bateria: el aviso de batería (<= 20 %) se muestra abajo en el Monitoreo ("! AVISO BATERIA 20%"), igual que las alarmas silenciadas |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "u8g2.h"
#include "encoder.h"

/*==================[macros]=================================================*/
#define UI_CODIGO_TECNICO   "4321"  /*!< Código técnico de 4 dígitos: en la clave del arranque abre el Modo Técnico (provisorio) */
#define UI_CLAVE_CLINICA    "1234"  /*!< Clave clínica de 4 dígitos, única para todo el servicio (provisoria, ver la especificación de inicio y navegación) */
#define UI_CHEQUEOS_MAX     5       /*!< Cantidad máxima de chequeos del autotest */

/*==================[typedef]================================================*/
/**
 * @brief Alarmas que puede mostrar la interfaz
 */
typedef enum {
    UI_ALARMA_NINGUNA = 0,
    UI_ALARMA_OCLUSION,           /*!< No se detectan gotas durante un tiempo configurable (flujo detenido) */
    UI_ALARMA_DESVIACION_FLUJO,   /*!< El caudal real se desvía más del 10 % del objetivo (por arriba o por debajo) */
    UI_ALARMA_FIN_SACHET,         /*!< Se alcanzó el volumen programado y ya no hay gotas */
    UI_ALARMA_BATERIA_BAJA,       /*!< Carga de la batería en estado crítico */
    UI_ALARMA_COLOCACION,         /*!< El equipo se movió de la guía (p. ej. por un choque): vuelve a la pantalla de colocación */
    UI_ALARMA_EQUIPO_REINICIADO,  /*!< El equipo se reinició con una infusión sin terminar: "El equipo se apagó — llamar a enfermería" */
    UI_ALARMA_GOTEO_SIN_MONITOREO,/*!< Caen gotas sin infusión activa (se detuvo el monitoreo pero el clamp quedó abierto): "Llamar a enfermería" */
} ui_alarma_t;

/**
 * @brief Resultado de ingresar una clave (para anotarlo en el registro)
 */
typedef enum {
    UI_CLAVE_OK,        /*!< Clave clínica correcta */
    UI_CLAVE_ERROR,     /*!< Clave incorrecta */
    UI_CLAVE_TECNICO,   /*!< Código técnico correcto (Modo Técnico) */
} ui_clave_resultado_t;

typedef void (*ui_clave_cb_t)(ui_clave_resultado_t resultado);   /*!< Se llama con cada clave ingresada */

/**
 * @brief Datos que muestra la interfaz. Los completa el main; la interfaz solo modifica
 * los de configuración (cuando se editan en pantalla) y alarma_silenciada, en_pausa e
 * infusion_configurada.
 */
typedef struct {
    // Configuración de la infusión (se edita en Config)
    uint16_t factor_goteo;         /*!< Gotas por mL del equipo (gtt/mL): 10, 15, 20 o 60 */
    uint16_t volumen_total_ml;     /*!< Volumen programado, en mL: 50, 100, 250, 500 o 1000 */
    uint16_t duracion_min;         /*!< Tiempo total programado, en minutos: de 10 min a 24 h, de a 10 min (se muestra como h:mm) */
    uint16_t flujo_objetivo_mlh;   /*!< Caudal objetivo en mL/h: lo calcula el equipo (volumen / tiempo), no se carga */
    uint16_t umbral_sin_gotas_s;   /*!< Segundos sin gotas para dar la alarma de oclusión */

    // Medición (los completa el sensado)
    uint16_t flujo_mlh;            /*!< Caudal medido, en mL/h */
    uint16_t volumen_ml;           /*!< Volumen infundido, en mL */
    uint32_t tiempo_s;             /*!< Tiempo de infusión, en segundos: la interfaz lo pone en 0 al empezar el Monitoreo (después del Ajuste de goteo); el main lo hace avanzar */
    uint16_t segundos_sin_gotas;   /*!< Segundos que lleva sin detectar gotas */
    float    angulo_grados;        /*!< Ángulo del equipo respecto de la vertical */
    bool     colocacion_ok;        /*!< true si el ángulo está dentro de la tolerancia */
    float    tolerancia_grados;    /*!< Rango correcto del ángulo (+/- este valor); la pantalla de colocación lo muestra */
    uint8_t  bateria_pct;          /*!< Carga de la batería, 0 a 100 */
    int16_t  desvio_pct;           /*!< Desvío del caudal respecto del objetivo, en %: positivo = más rápido (lo muestra la alarma de desvío) */

    // Estado
    ui_alarma_t alarma;            /*!< Alarma activa (UI_ALARMA_NINGUNA si no hay) */
    bool alarma_silenciada;        /*!< true si se la silenció con "Pulsar" (queda un aviso en el Monitoreo) */
    bool en_pausa;                 /*!< true si la infusión está pausada */
    // Modo Técnico (los completa el main para el Diagnóstico; la interfaz pide probar alarmas y borrar el registro)
    uint16_t base_optica_mv[2];    /*!< Nivel base de los dos fotodiodos, en mV */
    uint16_t bateria_mv;           /*!< Tensión de la batería, en mV */
    uint16_t registro_lineas;      /*!< Líneas que tiene el registro */
    uint16_t registro_minutos_libres; /*!< Minutos de infusión para los que todavía alcanza la memoria del registro: el Resumen no deja iniciar una duración mayor ("Memoria llena") */
    bool     registro_descargado;  /*!< true si la app ya descargó el registro (si no, el borrado pide confirmar el aviso) */
    const char *version;           /*!< Versión del firmware */
    uint8_t  probar_alarma;        /*!< Probar alarma: 0 = ninguna, n = número de patrón + 1 (el main lo hace sonar) */
    bool     borrar_registro;      /*!< La interfaz lo pone en true al confirmar el borrado; el main borra y lo baja */
    bool registro_pendiente;       /*!< true si hay un registro (infusión terminada o interrumpida) sin descargar: aviso "Registro pendiente de descarga", sin sonido */
    bool descartar_pendiente;      /*!< La interfaz lo pone en true cuando la enfermera elige "Continuar igual" con un registro sin descargar; el main lo baja */
    bool recordatorio_ajuste;      /*!< true si pasaron 10 min en el Ajuste de goteo sin confirmar: el pie dice "Confirmar ajuste" (el main hace el beep) */
    bool aviso_bateria;            /*!< true con el aviso de batería (carga <= 20 %): queda un aviso en blanco abajo en el Monitoreo, sin sonido ni pantalla de alarma */
    bool infusion_configurada;     /*!< true desde que se confirma el Resumen hasta que finaliza */
} ui_datos_t;

/**
 * @brief Resultado de un chequeo del autotest
 */
typedef enum {
    UI_CHEQUEO_PENDIENTE,   /*!< Todavía no se chequeó (se muestra "..") */
    UI_CHEQUEO_OK,          /*!< Respondió bien (se muestra "OK") */
    UI_CHEQUEO_FALLA,       /*!< No respondió (se muestra "ERR") */
} ui_chequeo_estado_t;

/**
 * @brief Un ítem del autotest
 */
typedef struct {
    const char *nombre;         /*!< Nombre que se muestra si falla ("Falla: <nombre>"), p. ej. "Acelerometro" */
    ui_chequeo_estado_t estado; /*!< Resultado actual */
    const char *accion;         /*!< Qué hacer si falla, p. ej. "Conectar cargador" (NULL = "No usar") */
} ui_chequeo_t;

/**
 * @brief Pantallas a las que se navega con el encoder (el Inicio es aparte: UI_PantallaInicio)
 */
typedef enum {
    UI_PANTALLA_AUTOTEST,       /*!< Resumen de los chequeos. Pulsar: seguir */
    UI_PANTALLA_COLOCACION,     /*!< Ángulo del equipo en la guía. Pulsar: seguir (solo si es correcto). Durante la infusión, aparece por la alarma de colocación y se cierra sola al resolverse */
    UI_PANTALLA_ACCESO,         /*!< Acceso técnico: código de 4 dígitos */
    UI_PANTALLA_CONFIG,         /*!< Un parámetro por pantalla: factor de goteo, volumen y tiempo total */
    UI_PANTALLA_RESUMEN,        /*!< Confirmación de la configuración. Pulsar: iniciar */
    UI_PANTALLA_AJUSTE_GOTEO,   /*!< Informa las gotas/min objetivo para regular el clamp; la enfermera lo hace sola. Pulsar: seguir */
    UI_PANTALLA_MONITOREO,      /*!< Caudal, volumen y tiempo (la pantalla "En uso") */
    UI_PANTALLA_ALARMA,         /*!< Alarma activa. Pulsar: silenciar */
    UI_PANTALLA_AJUSTES,        /*!< Ajustes (con clave): Cambiar datos, Detener monitoreo, Apagar y Volver */
    UI_PANTALLA_DETENER,        /*!< Confirmar "Finalizar infusión?" (Detener monitoreo) */
    UI_PANTALLA_FIN,            /*!< Sin infusión activa. Con la sesión abierta: Nueva infusión o Apagar; a los 60 s sin actividad queda bloqueada (mantener 3 s + clave) */
    UI_PANTALLA_APAGAR,         /*!< Confirmar apagado (con un registro sin descargar avisa antes) */
    UI_PANTALLA_PENDIENTE,      /*!< "Hay un registro sin descargar. ¿Continuar igual?" antes de configurar una infusión nueva */
    UI_PANTALLA_TECNICO,        /*!< Modo Técnico: Diagnóstico, Probar alarma, Borrar registro y Salir (solo en el arranque normal) */
    UI_PANTALLA_DIAGNOSTICO,    /*!< Modo Técnico: datos en vivo (solo lectura) */
    UI_PANTALLA_PROBAR_ALARMA,  /*!< Modo Técnico: hace sonar el patrón de cada alarma */
    UI_PANTALLA_BORRAR_REGISTRO,/*!< Modo Técnico: confirmar el borrado del registro */
} ui_pantalla_t;

/*==================[external functions declaration]=========================*/

/** @fn bool UI_Init(void)
 * @brief Inicia el I2C y la OLED y la deja en blanco.
 * @return true si el bus I2C quedó iniciado y la OLED respondió (usa el componente display)
 */
bool UI_Init(void);

/** @fn void UI_PantallaInicio(uint8_t cuadro)
 * @brief Dibuja la pantalla de inicio ("Iniciando" con una animación de espera)
 * @param cuadro Número de cuadro de la animación (28 cuadros, se repite); en Lopaka avanza cada 42 ms
 */
void UI_PantallaInicio(uint8_t cuadro);

/** @fn void UI_SetChequeos(const ui_chequeo_t *chequeos, uint8_t cantidad)
 * @brief Carga los resultados que muestra la pantalla Autotest (la lista debe seguir existiendo)
 */
void UI_SetChequeos(const ui_chequeo_t *chequeos, uint8_t cantidad);

/** @fn bool UI_SalirPedido(void)
 * @brief true si se eligió "Salir" del Modo Técnico o pasaron 5 min sin actividad: el main reinicia el equipo
 */
bool UI_SalirPedido(void);

/** @fn void UI_PantallaMensaje(const char *texto)
 * @brief Muestra un mensaje corto centrado (p. ej. "Registro guardado" antes del reposo)
 */
void UI_PantallaMensaje(const char *texto);

/** @fn void UI_SetTiempos(uint16_t inactividad_s, uint16_t tecnico_inactividad_s, uint16_t clave_bloqueo_s)
 * @brief Acorta para probar los tiempos de la interfaz: inactividad de la clave, Ajustes y Sin infusión (60 s en la
 * especificación), inactividad del Modo Técnico (300 s) y bloqueo de la clave (60 s). Un 0 deja el de la especificación.
 */
void UI_SetTiempos(uint16_t inactividad_s, uint16_t tecnico_inactividad_s, uint16_t clave_bloqueo_s);

/** @fn void UI_SetCallbackClave(ui_clave_cb_t cb)
 * @brief Registra la función que se llama con cada clave ingresada (NULL para quitarla)
 */
void UI_SetCallbackClave(ui_clave_cb_t cb);

/** @fn bool UI_EditandoDatos(void)
 * @brief true mientras se cambia la duración desde Ajustes (pantallas Config y Resumen): el monitoreo
 * sigue y las alarmas se siguen evaluando.
 */
bool UI_EditandoDatos(void);

/** @fn void UI_IniciarSinInfusion(ui_datos_t *datos)
 * @brief Arranca en "Sin infusión activa" (bloqueada, con el aviso del registro pendiente) en lugar de la clave del
 * arranque. Es lo que se muestra al encender con una infusión interrumpida o con un registro sin descargar.
 */
void UI_IniciarSinInfusion(ui_datos_t *datos);

/** @fn void UI_IniciarClave(ui_datos_t *datos)
 * @brief Muestra la clave del arranque (después del autotest). Al ingresarla bien sigue la
 * Colocación. No se puede cancelar y no vence por inactividad. Tres errores seguidos bloquean 60 s.
 */
void UI_IniciarClave(ui_datos_t *datos);

/** @fn void UI_Ir(ui_pantalla_t destino, ui_datos_t *datos)
 * @brief Salta a una pantalla y la dibuja (p. ej. UI_PANTALLA_AUTOTEST al terminar el Inicio)
 */
void UI_Ir(ui_pantalla_t destino, ui_datos_t *datos);

/** @fn void UI_Evento(encoder_evento_t evento, ui_datos_t *datos)
 * @brief Maneja un evento del encoder según la pantalla actual y la redibuja.
 * @param evento Evento sacado de Encoder_Leer()
 * @param datos Datos compartidos con el resto del sistema (la interfaz puede modificarlos)
 */
void UI_Evento(encoder_evento_t evento, ui_datos_t *datos);

/** @fn void UI_Actualizar(ui_datos_t *datos)
 * @brief Llamar cada ~1 s: pasa a la pantalla de una alarma que se activó o vuelve al
 * Monitoreo si se apagó, hace parpadear la alarma y redibuja con los datos nuevos.
 */
void UI_Actualizar(ui_datos_t *datos);

/** @fn ui_pantalla_t UI_PantallaActual(void)
 * @brief Pantalla en la que está la interfaz
 */
ui_pantalla_t UI_PantallaActual(void);

/** @fn bool UI_ApagarPedido(void)
 * @brief true si el usuario confirmó "Apagar equipo? Si". El main debe apagar el equipo.
 */
bool UI_ApagarPedido(void);

/** @fn void UI_PantallaApagada(void)
 * @brief Deja la OLED apagada (modo de bajo consumo).
 */
void UI_PantallaApagada(void);

/** @} */
#endif /* UI_H */
