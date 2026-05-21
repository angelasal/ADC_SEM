// main.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

// Importamos las cabeceras de tus módulos
#include "audio_processor.h"
#include "serial_manager.h" // ¡Cambiado! Ahora incluimos el gestor del puerto serie

static const char *TAG = "MAIN_SYSTEM";

void app_main(void) {
    // NOTA: Hemos eliminado la inicialización de NVS (nvs_flash) 
    // ya que el puerto serie no necesita memoria no volátil para arrancar.

    ESP_LOGI(TAG, "Iniciando Sistema de Monitorización Acústica por Puerto Serie...");

    // 2. CREAR LA COLA (Comunicación entre módulos)
    /* Creamos una cola para pasar los niveles de dB del audio al puerto serie.
       Esto permite que el audio siga midiendo en tiempo real mientras la UART transmite. */
    QueueHandle_t audio_to_serial_queue = xQueueCreate(10, sizeof(float));

    if (audio_to_serial_queue == NULL) {
        ESP_LOGE(TAG, "Error al crear la cola de datos");
        return;
    }

    // 3. INICIALIZAR HARDWARE
    audio_init();   // Configura el ADC continuo y DMA en el audio_processor.c
    serial_init();  // ¡Cambiado! Configura el puerto UART en serial_manager.c

    // 4. LANZAR TAREAS (Multiprocesamiento con FreeRTOS)
    /* Lanzamos las tareas de forma independiente para que el muestreo del ADC 
       no sufra retrasos por los tiempos de transmisión de la UART. */
    
    // Tarea de Audio: Prioridad alta (5) para asegurar el ritmo de muestreo
    xTaskCreate(audio_task, "Audio_Task", 4096, audio_to_serial_queue, 5, NULL);

    // Tarea de Serie: Prioridad media/alta (5) para escupir los datos al PC
    xTaskCreate(serial_task, "Serial_Task", 4096, audio_to_serial_queue, 5, NULL);

    ESP_LOGI(TAG, "Tareas de audio y transmisión serie lanzadas correctamente.");
}