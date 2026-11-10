/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "protocolo.h"
#include "registro.h"
#include "alarmas.h"
#include "esp_log.h"

/*==================[macros and definitions]=================================*/
#define TAG              "protocolo"
#define ORDEN_MAX        64     /*!< Largo máximo de una orden (con el '\n') */
#define LINEA_ENVIO_MAX  128    /*!< Largo máximo de una línea que se envía */
#define PACIENTE_MIN     2
#define PACIENTE_MAX     20
#define HORA_DIGITOS_MAX 12

/*==================[internal data definition]===============================*/
static const protocolo_enlace_t *enlace = NULL;
static char orden[ORDEN_MAX];
static size_t orden_largo = 0;
static bool orden_descartada = false;     /*!< La orden se pasó del largo: se ignora hasta el próximo '\n' */
static bool ok_pendiente = false;         /*!< Hubo una descarga completa y la app todavía no la confirmó */
static bool solo_contar = false;          /*!< true: Enviar no envía nada (se usa para contar las líneas de una descarga) */

/*==================[internal functions definition]==========================*/
/** Envía una línea de texto (se le agrega el '\n'). false si se cortó la conexión */
static bool Enviar(const char *formato, ...) {
    if (solo_contar) {
        return true;
    }
    char linea[LINEA_ENVIO_MAX];
    va_list args;
    va_start(args, formato);
    int n = vsnprintf(linea, sizeof(linea) - 1, formato, args);
    va_end(args);
    if (n < 0) {
        return false;
    }
    if (n > (int)sizeof(linea) - 2) {
        n = sizeof(linea) - 2;
    }
    linea[n++] = '\n';
    return enlace->enviar((const uint8_t *)linea, (size_t)n);
}

/** Duración en horas como pide la app: entera (8) o con decimales sin ceros de más (7.5, 1.667) */
static void FormatearHoras(uint16_t minutos, char *texto, size_t tam) {
    if (minutos % 60 == 0) {
        snprintf(texto, tam, "%u", minutos / 60);
        return;
    }
    snprintf(texto, tam, "%.3f", (double)minutos / 60.0);
    size_t n = strlen(texto);
    while (n > 0 && texto[n - 1] == '0') {
        texto[--n] = '\0';
    }
    if (n > 0 && texto[n - 1] == '.') {
        texto[n - 1] = '\0';
    }
}

/** Nombre del aviso o la alarma como lo espera la app. La dirección (0 = alto, 1 = bajo) solo importa en los desvíos */
static void NombreEvento(uint16_t evento, uint16_t direccion, char *texto, size_t tam) {
    const char *sentido = direccion ? "bajo" : "alto";
    switch (evento) {
    case ALARMA_VOLUMEN_COMPLETO:      snprintf(texto, tam, "Volumen completo"); break;
    case ALARMA_FLUJO_DETENIDO:        snprintf(texto, tam, "Flujo detenido"); break;
    case ALARMA_FIN_SACHET:            snprintf(texto, tam, "Fin de sachet"); break;
    case ALARMA_COLOCACION:            snprintf(texto, tam, "Colocación"); break;
    case ALARMA_EQUIPO_REINICIADO:     snprintf(texto, tam, "Equipo reiniciado"); break;
    case ALARMA_BATERIA_CRITICA:       snprintf(texto, tam, "Batería crítica"); break;
    case ALARMA_DESVIACION_FUERTE:     snprintf(texto, tam, "Desviación fuerte %s", sentido); break;
    case ALARMA_GOTEO_SIN_MONITOREO:   snprintf(texto, tam, "Goteo sin monitoreo"); break;
    case ALARMA_BATERIA_BAJA:          snprintf(texto, tam, "Batería baja"); break;
    case AVISO_DESVIACION_LEVE:        snprintf(texto, tam, "Desviación leve %s", sentido); break;
    case AVISO_PROXIMO_A_FINALIZAR:    snprintf(texto, tam, "Próximo a finalizar"); break;
    case AVISO_BATERIA:                snprintf(texto, tam, "Batería aviso"); break;
    default:                           snprintf(texto, tam, "Evento %u", evento); break;
    }
}

/** Convierte a texto y envía las líneas de la infusión que va de "inicio" a "total" (hasta su FIN). Cuenta cuántas son y en
 * qué minuto terminó. Con solo_contar no envía nada. Devuelve false si se cortó la conexión a mitad */
static bool Recorrer(uint32_t inicio, uint32_t total, uint8_t estado, uint32_t *cantidad_sal, uint32_t *minuto_fin_sal) {
    uint32_t cantidad = 0, minuto_fin = 0;
    bool termino = false;
    char nombre[40], horas[16];

    // Infusión interrumpida por un reinicio: no tiene línea FIN y lo que se envía termina en el último REINICIO
    if (estado == 2) {
        uint32_t limite = inicio;
        for (uint32_t j = inicio; j < total; j++) {
            registro_linea_t r;
            if (Registro_Leer(j, &r) && r.tipo == REG_TIPO_EVENTO && r.evento == REG_REINICIO) {
                limite = j + 1;
            }
        }
        if (limite > inicio) {
            total = limite;
        }
    }

    for (uint32_t i = inicio; i < total && !termino; i++) {
        registro_linea_t l, s;
        if (!Registro_Leer(i, &l) || l.tipo != REG_TIPO_EVENTO) {
            continue;
        }
        bool ok = true;
        switch (l.evento) {
        case REG_INICIO:
            // La duración viene en la línea siguiente (REG_DURACION)
            horas[0] = '\0';
            if (i + 1 < total && Registro_Leer(i + 1, &s) && s.tipo == REG_TIPO_EVENTO && s.evento == REG_DURACION) {
                FormatearHoras(s.a, horas, sizeof(horas));
                i++;
            }
            ok = Enviar("INICIO, ID=%lu, FG=%u, V=%u mL, T=%s h", (unsigned long)l.gotas, l.a, l.b, horas) &&
                 Enviar("0 min - Infusión iniciada");
            cantidad += 2;
            break;
        case REG_CAMBIO_DATOS:
            FormatearHoras(l.b, horas, sizeof(horas));
            ok = Enviar("%lu min - Cambio de datos T=%s h", (unsigned long)l.minuto, horas);
            cantidad++;
            break;
        case REG_REINICIO:
            ok = Enviar("%lu min - REINICIO", (unsigned long)l.minuto);   // el minuto es el último que se guardó antes del reinicio
            cantidad++;
            break;
        case REG_CLAVE_OK:
            ok = Enviar("%lu min - Clave correcta", (unsigned long)l.minuto);
            cantidad++;
            break;
        case REG_CLAVE_ERROR:
            ok = Enviar("%lu min - Clave incorrecta", (unsigned long)l.minuto);
            cantidad++;
            break;
        case REG_TECNICO_OK:
            ok = Enviar("%lu min - Acceso técnico", (unsigned long)l.minuto);
            cantidad++;
            break;
        case REG_ALARMA:
            NombreEvento(l.a, l.b, nombre, sizeof(nombre));
            if (l.activo) {
                ok = Enviar("%lu min - %s %s", (unsigned long)l.minuto, (l.a < AVISO_DESVIACION_LEVE) ? "ALARMA" : "AVISO", nombre);
            } else {
                ok = Enviar("%lu min - Alarma resuelta %s", (unsigned long)l.minuto, nombre);
            }
            cantidad++;
            break;
        case REG_SILENCIADO:
            NombreEvento(l.a, l.b, nombre, sizeof(nombre));
            ok = Enviar("%lu min - Alarma silenciada %s", (unsigned long)l.minuto, nombre);
            cantidad++;
            break;
        case REG_FIN:
            ok = Enviar("FIN, %lu gotas, %u mL estimados", (unsigned long)l.gotas, l.a);
            minuto_fin = l.minuto;
            termino = true;
            cantidad++;
            break;
        default:
            break;   // REG_AJUSTE_OK, REG_DURACION suelta y REG_DESCARGA no viajan
        }
        if (!ok) {
            ESP_LOGW(TAG, "descarga cortada: se perdio la conexion");
            return false;
        }
    }
    *cantidad_sal = cantidad;
    *minuto_fin_sal = minuto_fin;
    return true;
}

/** Envía la última infusión como texto y cierra con FIN_TX. Devuelve false si se cortó la conexión a mitad */
static bool Descargar(void) {
    uint32_t id = 0, inicio = 0;
    if (!Registro_UltimaInfusion(&id, &inicio)) {
        return Enviar("ERR,SIN_REGISTRO");
    }
    const uint32_t total = Registro_Total();   // lo que se escriba mientras se envía va en la próxima descarga
    const uint8_t estado = enlace->estado();
    uint32_t cantidad = 0, minuto_fin = 0;

    ESP_LOGI(TAG, "descarga: infusion ID=%lu (estado %u), lineas %lu a %lu", (unsigned long)id, estado, (unsigned long)inicio, (unsigned long)total);
    if (!Recorrer(inicio, total, estado, &cantidad, &minuto_fin)) {
        return false;
    }
    uint32_t minuto = enlace->minuto_actual();   // el firmware lo sigue contando después del fin hasta el OK (interrumpida: desde el reinicio)
    if (!Enviar("FIN_TX,%lu,%lu", (unsigned long)cantidad, (unsigned long)minuto)) {
        return false;
    }
    ok_pendiente = true;
    ESP_LOGI(TAG, "descarga enviada: %lu lineas, minuto %lu", (unsigned long)cantidad, (unsigned long)minuto);
    return true;
}

/** Estado de la última infusión, sin descargar nada: ESTADO,<ID>,<estado>,<minuto actual>,<líneas de una descarga>.
 * El estado es 1 = en curso, 0 = terminada, 2 = interrumpida por un reinicio. Con el minuto actual la app calcula la hora de
 * inicio (hora del celular menos el minuto actual) aunque se conecte tarde; si está interrumpida, son los minutos desde el reinicio */
static void Estado(void) {
    uint32_t id = 0, inicio = 0, cantidad = 0, minuto_fin = 0;
    if (!Registro_UltimaInfusion(&id, &inicio)) {
        Enviar("ERR,SIN_REGISTRO");
        return;
    }
    const uint8_t estado = enlace->estado();
    solo_contar = true;   // se cuentan las líneas que enviaría DESCARGAR sin enviarlas
    Recorrer(inicio, Registro_Total(), estado, &cantidad, &minuto_fin);
    solo_contar = false;
    Enviar("ESTADO,%lu,%u,%lu,%lu", (unsigned long)id, estado, (unsigned long)enlace->minuto_actual(), (unsigned long)cantidad);
}

/** La app confirma que recibió el registro completo: se anota DESCARGA y se marca como descargado */
static void Confirmar(void) {
    if (!ok_pendiente) {
        Enviar("ERR,OK");
        return;
    }
    ok_pendiente = false;
    // Primero se anota (anotar marca el registro como no descargado) y después se lo marca como descargado
    Registro_Anotar(REG_DESCARGA, 0, enlace->minuto_actual(), 0, 0);
    Registro_MarcarDescargado();
    if (enlace->descargado != NULL) {
        enlace->descargado();   // el firmware deja de contar el minuto y apaga el aviso y la alarma de reinicio
    }
    Enviar("ACK,OK");
}

static bool CodigoPacienteValido(const char *codigo) {
    size_t n = strlen(codigo);
    if (n < PACIENTE_MIN || n > PACIENTE_MAX) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        char c = codigo[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool SoloDigitos(const char *texto, size_t maximo) {
    size_t n = strlen(texto);
    if (n == 0 || n > maximo) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (texto[i] < '0' || texto[i] > '9') {
            return false;
        }
    }
    return true;
}

static void Ejecutar(char *texto) {
    ESP_LOGI(TAG, "orden: %s", texto);
    if (strncmp(texto, "HORA=", 5) == 0) {
        // Se acepta pero no se usa: el equipo no guarda la hora real (la app la calcula con el minuto actual de FIN_TX)
        Enviar(SoloDigitos(texto + 5, HORA_DIGITOS_MAX) ? "ACK,HORA" : "ERR,HORA");
    } else if (strncmp(texto, "PACIENTE=", 9) == 0) {
        // Se valida el formato y se descarta: el paciente queda solo en la app
        Enviar(CodigoPacienteValido(texto + 9) ? "ACK,PACIENTE" : "ERR,PACIENTE");
    } else if (strcmp(texto, "ESTADO") == 0) {
        Estado();
    } else if (strcmp(texto, "DESCARGAR") == 0) {
        ok_pendiente = false;
        Descargar();
    } else if (strcmp(texto, "OK") == 0) {
        Confirmar();
    } else {
        Enviar("ERR,COMANDO");
    }
}

/*==================[external functions definition]==========================*/
void Protocolo_Init(const protocolo_enlace_t *enlace_externo) {
    enlace = enlace_externo;
    Protocolo_Reiniciar();
}

void Protocolo_Reiniciar(void) {
    orden_largo = 0;
    orden_descartada = false;
    ok_pendiente = false;
}

void Protocolo_FinDeEntrada(void) {
    if (enlace != NULL && orden_largo > 0 && !orden_descartada) {
        orden[orden_largo] = '\0';
        orden_largo = 0;
        Ejecutar(orden);
    }
}

void Protocolo_Recibir(const uint8_t *datos, size_t largo) {
    for (size_t i = 0; i < largo && enlace != NULL; i++) {
        char c = (char)datos[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (orden_descartada) {
                Enviar("ERR,COMANDO");
            } else if (orden_largo > 0) {
                orden[orden_largo] = '\0';
                Ejecutar(orden);
            }
            orden_largo = 0;
            orden_descartada = false;
        } else if (orden_largo < ORDEN_MAX - 1) {
            orden[orden_largo++] = c;
        } else {
            orden_descartada = true;   // demasiado larga: se ignora hasta el '\n'
        }
    }
}
