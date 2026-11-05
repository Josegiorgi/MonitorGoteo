/*==================[inclusions]=============================================*/
#include <string.h>
#include "registro.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_timer.h"

/*==================[macros and definitions]=================================*/
#define NAMESPACE        "registro"
#define CLAVE_FORMATO    "formato"      /*!< 1 = la partición ya se borró y se está usando */
#define CLAVE_DESCARGA   "descargado"
#define CLAVE_ACTIVA     "activa"
#define PARTICION_NOMBRE "registro"
#define PARTICION_SUBTIPO 0x40          /*!< Subtipo propio (tipo data) de partitions.csv */
#define TIPO_VACIA       0xFF           /*!< Una línea sin escribir está borrada (0xFF) */
#define MARGEN_LINEAS    50             /*!< Líneas que se reservan para eventos al calcular los minutos libres */

_Static_assert(sizeof(registro_linea_t) == REGISTRO_LINEA_B, "la linea del registro debe medir 16 bytes");

/*==================[internal data definition]===============================*/
static const char *TAG = "registro";
static nvs_handle_t h;
static const esp_partition_t *particion = NULL;
static bool listo = false;
static uint32_t capacidad = 0;        /*!< Líneas que entran en la partición */
static uint32_t total = 0;            /*!< Líneas escritas (la próxima va en esta posición) */
static bool descargado = true;        /*!< Sin líneas no hay nada para descargar */

static const char *const nombres[] = {
    "?", "INICIO", "DURACION", "AJUSTE_OK", "CAMBIO_DATOS", "FIN", "REINICIO", "CLAVE_OK",
    "CLAVE_ERROR", "TECNICO_OK", "ALARMA", "SILENCIADO", "DESCARGA"};

/*==================[internal functions definition]==========================*/
static bool LeerLinea(uint32_t indice, registro_linea_t *l) {
    return esp_partition_read(particion, (size_t)indice * REGISTRO_LINEA_B, l, REGISTRO_LINEA_B) == ESP_OK;
}

/** Busca la primera línea sin escribir con una búsqueda binaria: lo escrito es siempre una sucesión sin huecos */
static uint32_t BuscarFinal(void) {
    uint32_t bajo = 0, alto = capacidad;   // la primera sin escribir está en [bajo, alto]
    registro_linea_t l;
    while (bajo < alto) {
        uint32_t medio = bajo + (alto - bajo) / 2;
        if (LeerLinea(medio, &l) && l.tipo == TIPO_VACIA) {
            alto = medio;
        } else {
            bajo = medio + 1;
        }
    }
    return bajo;
}

static void MarcarNoDescargado(void) {
    if (descargado) {
        descargado = false;
        nvs_set_u8(h, CLAVE_DESCARGA, 0);
        nvs_commit(h);
    }
}

static bool Agregar(const registro_linea_t *l) {
    if (!listo || particion == NULL || total >= capacidad) {
        return false;
    }
    if (esp_partition_write(particion, (size_t)total * REGISTRO_LINEA_B, l, REGISTRO_LINEA_B) != ESP_OK) {
        return false;
    }
    total++;
    MarcarNoDescargado();
    return true;
}

static bool BorrarParticion(void) {
    ESP_LOGW(TAG, "borrando la particion del registro (unos 10 s)...");
    return esp_partition_erase_range(particion, 0, particion->size) == ESP_OK;
}

/*==================[external functions definition]==========================*/
bool Registro_Init(void) {
    if (listo) {
        return true;
    }
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK || nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    particion = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)PARTICION_SUBTIPO, PARTICION_NOMBRE);
    if (particion == NULL) {
        ESP_LOGE(TAG, "no existe la particion '%s' (partitions.csv)", PARTICION_NOMBRE);
        return false;
    }
    capacidad = particion->size / REGISTRO_LINEA_B;

    uint8_t formato = 0;
    if (nvs_get_u8(h, CLAVE_FORMATO, &formato) != ESP_OK || formato != 1) {
        // Primera vez con esta partición: puede tener datos de otro firmware, se la deja en blanco
        if (!BorrarParticion()) {
            return false;
        }
        nvs_erase_key(h, CLAVE_ACTIVA);
        nvs_set_u8(h, CLAVE_FORMATO, 1);
        nvs_set_u8(h, CLAVE_DESCARGA, 1);
        nvs_commit(h);
    }

    total = BuscarFinal();
    uint8_t d = 1;
    if (nvs_get_u8(h, CLAVE_DESCARGA, &d) != ESP_OK) {
        d = (total == 0) ? 1 : 0;
    }
    descargado = (d != 0);
    listo = true;
    ESP_LOGI(TAG, "registro: %lu de %lu lineas escritas", (unsigned long)total, (unsigned long)capacidad);
    return true;
}

bool Registro_Probar(void) {
    if (!Registro_Init()) {
        return false;
    }
    registro_linea_t l;
    uint32_t escrito = (uint32_t)esp_timer_get_time(), leido = 0;
    return nvs_set_u32(h, "prueba", escrito) == ESP_OK && nvs_commit(h) == ESP_OK &&
           nvs_get_u32(h, "prueba", &leido) == ESP_OK && leido == escrito && LeerLinea(0, &l);
}

void Registro_Anotar(registro_evento_t evento, uint8_t activo, uint32_t minuto, uint16_t a, uint16_t b) {
    registro_linea_t l;
    memset(&l, 0, sizeof(l));
    l.minuto = minuto;
    l.tipo = REG_TIPO_EVENTO;
    l.evento = (uint8_t)evento;
    l.activo = activo;
    l.a = a;
    l.b = b;
    Agregar(&l);
}

void Registro_Muestra(uint32_t minuto, uint32_t gotas, uint16_t caudal_x10, uint16_t volumen_x10) {
    registro_linea_t l;
    memset(&l, 0, sizeof(l));
    l.minuto = minuto;
    l.tipo = REG_TIPO_MUESTRA;
    l.a = caudal_x10;
    l.b = volumen_x10;
    l.gotas = gotas;
    Agregar(&l);
}

uint32_t Registro_Total(void) {
    return total;
}

bool Registro_Leer(uint32_t indice, registro_linea_t *linea) {
    return listo && linea != NULL && indice < total && LeerLinea(indice, linea);
}

bool Registro_UltimaMuestra(uint32_t desde_linea, uint32_t *minuto, uint32_t *gotas) {
    registro_linea_t l;
    for (uint32_t i = total; i > desde_linea; i--) {
        if (LeerLinea(i - 1, &l) && l.tipo == REG_TIPO_MUESTRA) {
            if (minuto != NULL) {
                *minuto = l.minuto;
            }
            if (gotas != NULL) {
                *gotas = l.gotas;
            }
            return true;
        }
    }
    return false;
}

uint16_t Registro_MinutosLibres(void) {
    if (!listo || total >= capacidad) {
        return 0;
    }
    uint32_t libres = capacidad - total;
    uint32_t util = (libres > MARGEN_LINEAS) ? (libres - MARGEN_LINEAS) * 14 / 15 : 0;   // el resto queda para los eventos
    return (util > UINT16_MAX) ? UINT16_MAX : (uint16_t)util;
}

bool Registro_Borrar(void) {
    if (!listo || particion == NULL || !BorrarParticion()) {
        return false;
    }
    total = 0;
    descargado = true;
    nvs_erase_key(h, CLAVE_ACTIVA);
    nvs_set_u8(h, CLAVE_DESCARGA, 1);
    return nvs_commit(h) == ESP_OK;
}

bool Registro_Descargado(void) {
    return descargado;
}

void Registro_MarcarDescargado(void) {
    if (listo && !descargado) {
        descargado = true;
        nvs_set_u8(h, CLAVE_DESCARGA, 1);
        nvs_commit(h);
    }
}

bool Registro_GuardarInfusion(const registro_infusion_t *infusion) {
    return listo && infusion != NULL && nvs_set_blob(h, CLAVE_ACTIVA, infusion, sizeof(*infusion)) == ESP_OK &&
           nvs_commit(h) == ESP_OK;
}

bool Registro_CargarInfusion(registro_infusion_t *infusion) {
    if (!listo || infusion == NULL) {
        return false;
    }
    size_t tam = sizeof(*infusion);
    return nvs_get_blob(h, CLAVE_ACTIVA, infusion, &tam) == ESP_OK && tam == sizeof(*infusion);
}

void Registro_BorrarInfusion(void) {
    if (listo) {
        nvs_erase_key(h, CLAVE_ACTIVA);
        nvs_commit(h);
    }
}

const char *Registro_NombreEvento(uint8_t evento) {
    return (evento >= 1 && evento < sizeof(nombres) / sizeof(nombres[0])) ? nombres[evento] : nombres[0];
}
