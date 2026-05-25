#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

// Inclusión de tus cabeceras
#include "serial_manager.h"
#include "mqtt_service.h" // Módulo proporcionado en la asignatura [4]

// --- Definición de constantes (Estándar BARR-C) ---
#define UMBRAL_ALARMA       80.0f
#define MQTT_BROKER_URI     "mqtt://mqtt-dashboard.com"
// Cambia "tu_grupo" por el nombre de tu equipo para no pisar mensajes de otros compañeros
#define MQTT_TOPIC          "upv/giirob/tu_grupo/alertas" 
#define MAX_MSG_LENGTH      64

static const char *TAG = "TELEMETRIA_IOT";

// === FUNCIÓN INICIALIZACIÓN ===
void serial_init(void) 
{
    ESP_LOGI(TAG, "Inicializando conexión Wi-Fi y servicio MQTT...");
    // Suponiendo que el Wi-Fi ya se conecta en algún punto, inicializamos el servicio MQTT
    mqtt_service_init(MQTT_BROKER_URI);
}

// === TAREA EN SEGUNDO PLANO DE ENVÍO (Consumidor) ===
void serial_task(void *pvParameters) 
{
    // BARR-C: Inicialización de variables al inicio del bloque
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;
    float alerta_recibida = 0.0f;
    char data_str[MAX_MSG_LENGTH]; 
    int ret = 0;

    // Validación de punteros
    if (data_queue == NULL) 
    {
        ESP_LOGE(TAG, "Error: Cola nula en serial_task");
        return; 
    }

    for (;;) 
    {
        // La tarea duerme (0 consumo de CPU) hasta que llegue un dato en la cola [5, 6]
        if (xQueueReceive(data_queue, &alerta_recibida, portMAX_DELAY) == pdTRUE) 
        {
            // Solo publicamos si la IA supera nuestro umbral de confianza
            if (alerta_recibida >= UMBRAL_ALARMA) 
            {
                // Formateamos el mensaje en un JSON simple para fácil explotación [2]
                snprintf(data_str, sizeof(data_str), "{\"alerta\":\"llanto_detectado\", \"fiabilidad_ia\":%.2f}", alerta_recibida);
                
                // Publicamos en el broker MQTT [4]
                ret = mqtt_service_publish(MQTT_TOPIC, data_str, strlen(data_str));
                
                // Control de errores (mqtt_service_publish devuelve negativo si falla) [4, 7]
                if (ret < 0) 
                {
                    ESP_LOGW(TAG, "Aviso: No se pudo publicar en MQTT (¿Pérdida de red?)");
                } 
                else 
                {
                    ESP_LOGI(TAG, "Éxito. Alerta enviada a la nube: %s", data_str);
                }
            }
        }
    }
}