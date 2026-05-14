// main.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

void app_main(void) {
    // 1. Crear la cola para comunicar los módulos (Sincronización)
    QueueHandle_t audio_to_wifi_queue = xQueueCreate(10, sizeof(float));

    // 2. Inicializar Hardware
    audio_init();
    wifi_init();

    // 3. Lanzar Tareas (Multiprocesamiento) 
    xTaskCreate(audio_task, "Audio_Task", 4096, audio_to_wifi_queue, 5, NULL);
    xTaskCreate(cloud_task, "WiFi_Task", 4096, audio_to_wifi_queue, 5, NULL);
}