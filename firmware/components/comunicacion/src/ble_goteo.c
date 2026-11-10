/*==================[inclusions]=============================================*/
#include <string.h>
#include "ble_goteo.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/
#define TAG                     "ble_goteo"
#define APP_ID                  0x56
#define SERVICIO_INSTANCIA      0
#define UUID_SERVICIO           0xFFE0   /*!< Servicio (igual que un módulo HM-10) */
#define UUID_CARACTERISTICA     0xFFE1   /*!< Característica que sirve para escribir y para recibir notificaciones */
#define MTU_LOCAL               247      /*!< MTU que se propone; si la app no lo pide, queda en 23 (pedazos de 20 bytes) */
#define MTU_POR_DEFECTO         23
#define PAUSA_ENTRE_ENVIOS_MS   12       /*!< Pausa entre notificaciones para no saturar el buffer del Bluetooth */
#define REINTENTOS_ENVIO        5
#define ESPERA_CONGESTION_MS    2000     /*!< Lo máximo que se espera a que se libere la congestión */
#define ANUNCIO_MIN             0x0640   /*!< Intervalo del anuncio, en unidades de 0,625 ms: 1000 ms (~1 s, como pide el contrato) */
#define ANUNCIO_MAX             0x0700   /*!< 1120 ms */

/* Atributos de la base de datos del servicio: declaración del servicio, de la característica, su valor y su CCCD */
enum {
    IDX_SERVICIO,
    IDX_CARACT_DECL,
    IDX_CARACT_VALOR,
    IDX_CARACT_CCCD,
    IDX_TOTAL,
};

/*==================[internal data definition]===============================*/
static char nombre_equipo[BLE_GOTEO_NOMBRE_MAX + 1];
static QueueHandle_t cola_rx = NULL;
static uint16_t handles[IDX_TOTAL];
static esp_gatt_if_t gatts_if_actual = ESP_GATT_IF_NONE;
static uint16_t conn_id = 0;
static volatile bool conectado = false;
static volatile bool notificaciones = false;   /*!< La app activó las notificaciones (escribió el CCCD) */
static volatile bool congestionado = false;
static volatile uint16_t mtu = MTU_POR_DEFECTO;
static bool iniciado = false;
static uint8_t anuncio_crudo[31];
static uint8_t anuncio_largo = 0;

static const uint16_t uuid_servicio_primario = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t uuid_declaracion = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t uuid_cccd = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t uuid_servicio = UUID_SERVICIO;
static const uint16_t uuid_caracteristica = UUID_CARACTERISTICA;
static const uint8_t propiedades = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE |
                                   ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t valor_inicial[1] = {0};
static uint8_t cccd_inicial[2] = {0x00, 0x00};

/* Sin permisos de cifrado ni autenticación: la app se conecta sin emparejar (el prototipo no usa passkey) */
static const esp_gatts_attr_db_t base_de_datos[IDX_TOTAL] = {
    [IDX_SERVICIO] = {{ESP_GATT_AUTO_RSP},
                      {ESP_UUID_LEN_16, (uint8_t *)&uuid_servicio_primario, ESP_GATT_PERM_READ, sizeof(uint16_t),
                       sizeof(uint16_t), (uint8_t *)&uuid_servicio}},
    [IDX_CARACT_DECL] = {{ESP_GATT_AUTO_RSP},
                         {ESP_UUID_LEN_16, (uint8_t *)&uuid_declaracion, ESP_GATT_PERM_READ, sizeof(uint8_t),
                          sizeof(uint8_t), (uint8_t *)&propiedades}},
    [IDX_CARACT_VALOR] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, (uint8_t *)&uuid_caracteristica, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                           BLE_GOTEO_RX_MAX, sizeof(valor_inicial), valor_inicial}},
    [IDX_CARACT_CCCD] = {{ESP_GATT_AUTO_RSP},
                         {ESP_UUID_LEN_16, (uint8_t *)&uuid_cccd, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                          sizeof(uint16_t), sizeof(cccd_inicial), cccd_inicial}},
};

static esp_ble_adv_params_t parametros_anuncio = {
    .adv_int_min = ANUNCIO_MIN,
    .adv_int_max = ANUNCIO_MAX,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/*==================[internal functions definition]==========================*/
/** Arma el paquete de anuncio: banderas, servicio 0xFFE0 y el nombre completo (entra en 31 bytes) */
static void ArmarAnuncio(void) {
    uint8_t n = 0;
    anuncio_crudo[n++] = 0x02;   // banderas
    anuncio_crudo[n++] = 0x01;
    anuncio_crudo[n++] = 0x06;
    anuncio_crudo[n++] = 0x03;   // lista completa de servicios de 16 bits
    anuncio_crudo[n++] = 0x03;
    anuncio_crudo[n++] = UUID_SERVICIO & 0xFF;
    anuncio_crudo[n++] = UUID_SERVICIO >> 8;
    uint8_t largo_nombre = (uint8_t)strlen(nombre_equipo);
    anuncio_crudo[n++] = largo_nombre + 1;   // nombre completo
    anuncio_crudo[n++] = 0x09;
    memcpy(&anuncio_crudo[n], nombre_equipo, largo_nombre);
    n += largo_nombre;
    anuncio_largo = n;
}

static void EventoGap(esp_gap_ble_cb_event_t evento, esp_ble_gap_cb_param_t *param) {
    switch (evento) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&parametros_anuncio);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "no pudo empezar a anunciarse (error %x)", param->adv_start_cmpl.status);
        } else {
            ESP_LOGI(TAG, "anunciandose como %s", nombre_equipo);
        }
        break;
    default:
        break;
    }
}

static void EventoGatts(esp_gatts_cb_event_t evento, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (evento) {
    case ESP_GATTS_REG_EVT:
        if (param->reg.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "no se pudo registrar el servicio GATT (%d)", param->reg.status);
            break;
        }
        gatts_if_actual = gatts_if;
        esp_ble_gap_set_device_name(nombre_equipo);
        esp_ble_gap_config_adv_data_raw(anuncio_crudo, anuncio_largo);
        esp_ble_gatts_create_attr_tab(base_de_datos, gatts_if, IDX_TOTAL, SERVICIO_INSTANCIA);
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == IDX_TOTAL) {
            memcpy(handles, param->add_attr_tab.handles, sizeof(handles));
            esp_ble_gatts_start_service(handles[IDX_SERVICIO]);
        } else {
            ESP_LOGE(TAG, "no se pudo crear la tabla de atributos (estado %d, %d atributos)",
                     param->add_attr_tab.status, param->add_attr_tab.num_handle);
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        mtu = MTU_POR_DEFECTO;
        notificaciones = false;
        congestionado = false;
        conectado = true;
        ESP_LOGI(TAG, "app conectada");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        conectado = false;
        notificaciones = false;
        ESP_LOGI(TAG, "app desconectada");
        esp_ble_gap_start_advertising(&parametros_anuncio);   // vuelve a anunciarse para la próxima visita
        break;
    case ESP_GATTS_MTU_EVT:
        mtu = param->mtu.mtu;
        break;
    case ESP_GATTS_CONGEST_EVT:
        congestionado = param->congest.congested;
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.is_prep) {
            break;   // no se usan las escrituras largas: la app manda órdenes cortas
        }
        if (param->write.handle == handles[IDX_CARACT_CCCD]) {
            notificaciones = (param->write.len >= 2) && (param->write.value[0] & 0x01);   // la app activó o desactivó las notificaciones
        } else if (param->write.handle == handles[IDX_CARACT_VALOR] && cola_rx != NULL) {
            ble_goteo_rx_t rx;
            rx.largo = (param->write.len > BLE_GOTEO_RX_MAX) ? BLE_GOTEO_RX_MAX : (uint8_t)param->write.len;
            memcpy(rx.datos, param->write.value, rx.largo);
            xQueueSend(cola_rx, &rx, 0);   // si la cola está llena se pierde el pedazo
        }
        break;
    default:
        break;
    }
}

/*==================[external functions definition]==========================*/
bool BleGoteo_Init(const char *nombre, QueueHandle_t cola) {
    if (iniciado || nombre == NULL || cola == NULL) {
        return iniciado;
    }
    strncpy(nombre_equipo, nombre, BLE_GOTEO_NOMBRE_MAX);
    nombre_equipo[BLE_GOTEO_NOMBRE_MAX] = '\0';
    cola_rx = cola;
    ArmarAnuncio();

    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t e = esp_bt_controller_init(&config);
    if (e == ESP_OK) {
        e = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    }
    if (e == ESP_OK) {
        e = esp_bluedroid_init();
    }
    if (e == ESP_OK) {
        e = esp_bluedroid_enable();
    }
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo iniciar el Bluetooth (%s)", esp_err_to_name(e));
        return false;
    }
    esp_ble_gatts_register_callback(EventoGatts);
    esp_ble_gap_register_callback(EventoGap);
    esp_ble_gatts_app_register(APP_ID);
    esp_ble_gatt_set_local_mtu(MTU_LOCAL);
    iniciado = true;
    return true;
}

bool BleGoteo_Conectado(void) {
    return conectado;
}

bool BleGoteo_Enviar(const uint8_t *datos, size_t largo) {
    size_t enviados = 0;
    while (enviados < largo) {
        if (!conectado) {
            return false;
        }
        if (!notificaciones) {
            ESP_LOGW(TAG, "la app no activo las notificaciones de la caracteristica: no se envia la respuesta");
            return false;
        }
        for (int i = 0; congestionado && i < ESPERA_CONGESTION_MS / 10; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));   // el Bluetooth avisó que no puede más: se espera
        }
        size_t paso = (mtu > 3) ? (size_t)(mtu - 3) : 20;
        if (paso > BLE_GOTEO_RX_MAX) {
            paso = BLE_GOTEO_RX_MAX;
        }
        size_t k = (largo - enviados < paso) ? (largo - enviados) : paso;

        esp_err_t e = ESP_FAIL;
        for (int intento = 0; intento < REINTENTOS_ENVIO && e != ESP_OK; intento++) {
            e = esp_ble_gatts_send_indicate(gatts_if_actual, conn_id, handles[IDX_CARACT_VALOR], (uint16_t)k,
                                            (uint8_t *)(datos + enviados), false);
            if (e != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "falla al notificar (%s)", esp_err_to_name(e));
            return false;
        }
        enviados += k;
        vTaskDelay(pdMS_TO_TICKS(PAUSA_ENTRE_ENVIOS_MS));
    }
    return true;
}

void BleGoteo_Apagar(void) {
    if (!iniciado) {
        return;
    }
    esp_ble_gap_stop_advertising();
    if (conectado) {
        esp_ble_gatts_close(gatts_if_actual, conn_id);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    conectado = false;
    notificaciones = false;
    iniciado = false;
    ESP_LOGI(TAG, "Bluetooth apagado");
}
