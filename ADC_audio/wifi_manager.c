// wifi_manager.c
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

void wifi_init(void) {
    // Inicialización estándar de NVS y WiFi en modo Station
    // Aquí conectarías a la red de la UPV o de tu casa
    ESP_LOGI("WIFI", "Conectando a la red...");
}

// Tarea que recibe los datos de audio y los sube a un servidor IoT
void cloud_task(void *pvParameters) {
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;
    float received_db;

    while (1) {
        if (xQueueReceive(data_queue, &received_db, portMAX_DELAY)) {
            // Aquí usarías HTTP o MQTT para enviar 'received_db' a un Dashboard
            ESP_LOGI("CLOUD", "Enviando a Internet: %.2f dB", received_db);
        }
    }
}