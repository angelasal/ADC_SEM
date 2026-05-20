// wifi_manager.c
#include "wifi_manager.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WIFI_MQTT_MOD";
static esp_mqtt_client_handle_t client;

//--- CONFIGURACIÓN WI-FI ---
#define WIFI_SSID       "TU_SSID_DE_WIFI"     // <-- Pon aquí el nombre de tu red
#define WIFI_PASS       "TU_CONTRASENA_WIFI" // <-- Pon aquí la contraseña de tu red

// --- CONFIGURACIÓN MQTT ---
#define MQTT_BROKER_URL "mqtt://broker.emqx.io" 
#define MQTT_USERNAME   "sem_adc"
#define MQTT_PASSWORD   "sem_adc"
#define MQTT_TOPIC      "sem_adc/proyecto/rachetas"

// Manejador de eventos de MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al Broker MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del Broker MQTT");
            break;
        default:
            break;
    }
}

void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// === ESTO ES LO QUE TENÍAS QUE AÑADIR: MANEJADOR DE EVENTOS WI-FI ===
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Intentando reconectar al Wi-Fi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado! IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// === ESTO ES LO QUE TENÍAS QUE AÑADIR: FUNCIÓN INICIALIZACIÓN ===
void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void cloud_task(void *pvParameters) {
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;
    float db_received;
    char data_str[10];

    // Iniciamos la conexión MQTT (se asume que para cuando empiece el bucle ya habrá IP)
    mqtt_app_start();

    while (1) {
        if (xQueueReceive(data_queue, &db_received, portMAX_DELAY)) {
            snprintf(data_str, sizeof(data_str), "%.2f", db_received);
            int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, data_str, 0, 1, 0);
            ESP_LOGI(TAG, "Enviado a MQTT (ID:%d): %s dB", msg_id, data_str);
        }
    }
}