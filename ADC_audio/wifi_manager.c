#include "mqtt_client.h"
#include "esp_wifi.h"
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "MQTT_MOD";
static esp_mqtt_client_handle_t client;

// --- CONFIGURACIÓN MQTT ---
#define MQTT_BROKER_URL "mqtt://io.adafruit.com" // Ejemplo con Adafruit IO
#define MQTT_USERNAME   "TU_USUARIO_ADAFRUIT"
#define MQTT_PASSWORD   "TU_AIO_KEY"
#define MQTT_TOPIC      "TU_USUARIO/feeds/ruido-db"

// Manejador de eventos de MQTT (conexión, desconexión, etc.)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al Broker MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del Broker");
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

void cloud_task(void *pvParameters) {
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;
    float db_received;
    char data_str[10];

    // Iniciamos la conexión MQTT una vez que el WiFi esté listo
    mqtt_app_start();

    while (1) {
        // Esperamos el dato procesado del audio_task
        if (xQueueReceive(data_queue, &db_received, portMAX_DELAY)) {
            
            // Convertimos el float a string para enviarlo
            snprintf(data_str, sizeof(data_str), "%.2f", db_received);
            
            // PUBLICAR: Enviamos el dato al "Topic" para que aparezca en la gráfica
            int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, data_str, 0, 1, 0);
            ESP_LOGI(TAG, "Enviado a MQTT (ID:%d): %s dB", msg_id, data_str);
        }
    }
}