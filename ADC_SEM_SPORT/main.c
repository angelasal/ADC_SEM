#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "audio_processor.h"
#include "serial_manager.h"


#define QUEUE_LENGTH        10               // Capacidad máxima de la cola
#define ITEM_SIZE           sizeof(float)    // Tamaño de cada elemento
#define AUDIO_TASK_STACK    4096             // Tamaño de la pila para procesar la IA
#define SERIAL_TASK_STACK   4096             // Tamaño de la pila para enviar los datos
#define AUDIO_TASK_PRIO     5                // Prioridad alta para no perder muestras de audio
#define SERIAL_TASK_PRIO    4                // Prioridad menor para las comunicaciones

static const char *TAG = "MAIN_SYSTEM";

void app_main(void) 
{
    QueueHandle_t data_queue = NULL;

    ESP_LOGI(TAG, "Iniciando sistema detector...");

    serial_init();
    audio_init();

    data_queue = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    
    if (data_queue == NULL) 
    {
        ESP_LOGE(TAG, "Error: No se puede asignar memoria para la cola.");
        return; // BARR-C prohíbe usar saltos bruscos como abort(), salimos de forma segura
    }

    // 4. Creación de las tareas, inyectándoles el 'handle' de la cola
    if (xTaskCreate(audio_task, "audio_task", AUDIO_TASK_STACK, (void *)data_queue, AUDIO_TASK_PRIO, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Error: No se puede crear audio_task");
    }

    if (xTaskCreate(serial_task, "serial_task", SERIAL_TASK_STACK, (void *)data_queue, SERIAL_TASK_PRIO, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Error: No se puede crear serial_task");
    }

    // main puede terminar aquí, las tareas en segundo plano siguen
    ESP_LOGI(TAG, "Funcionando con éxito, main finaliza.");
}