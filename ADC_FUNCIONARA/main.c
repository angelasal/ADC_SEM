#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_log.h"

// Importamos las cabeceras de tus módulos (deberás crear los .h)
#include "audio_processor.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN_SYSTEM";

void app_main(void) {
    // 1. INICIALIZAR NVS (Obligatorio para el WiFi en ESP32)
    /* El WiFi necesita guardar datos de configuración en la memoria no volátil */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Iniciando Sistema de Monitorización Acústica...");

    // 2. CREAR LA COLA (Comunicación entre módulos)
    /* Creamos una cola para pasar los niveles de dB del audio al WiFi.
       Esto permite que el audio siga midiendo mientras el WiFi envía datos. */
    QueueHandle_t audio_to_wifi_queue = xQueueCreate(10, sizeof(float));

    if (audio_to_wifi_queue == NULL) {
        ESP_LOGE(TAG, "Error al crear la cola de datos");
        return;
    }

    // 3. INICIALIZAR HARDWARE
    audio_init(); // Configura el ADC continuo y DMA en el audio_processor.c
    wifi_init();  // Configura la conexión WiFi en el wifi_manager.c

    // 4. LANZAR TAREAS (Multiprocesamiento con FreeRTOS)
    /* Lanzamos las tareas en núcleos o con prioridades para que el ADC 
       no se detenga por culpa de la lentitud de Internet. */
    
    // Tarea de Audio: Prioridad alta (5) para no perder muestras (Nyquist)
    xTaskCreate(audio_task, "Audio_Task", 4096, audio_to_wifi_queue, 5, NULL);

    // Tarea de WiFi: Prioridad media (5) para exportar datos a Internet
    xTaskCreate(cloud_task, "WiFi_Task", 4096, audio_to_wifi_queue, 5, NULL);

    ESP_LOGI(TAG, "Tareas lanzadas correctamente.");
    
}