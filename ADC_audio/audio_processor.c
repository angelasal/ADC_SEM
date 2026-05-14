// audio_processor.c
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>

#define SAMPLE_FREQ_HZ 16000 // Frecuencia para capturar voz/sonido ambiental
#define READ_LEN 1024        // Tamaño del buffer para el procesamiento

static adc_continuous_handle_t adc_handle = NULL;

// Función para inicializar el ADC en modo continuo (DMA)
void audio_init(void) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    };
    
    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL_2 & 0x7, // Pin del micrófono
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
}

// Tarea que calcula el volumen promedio (RMS) para detectar ruido
void audio_task(void *pvParameters) {
    uint8_t result[READ_LEN];
    uint32_t ret_num = 0;
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;

    adc_continuous_start(adc_handle);

    while (1) {
        if (adc_continuous_read(adc_handle, result, READ_LEN, &ret_num, 0) == ESP_OK) {
            // Lógica para calcular la intensidad del sonido (RMS)
            float sum_sq = 0;
            int count = ret_num / SOC_ADC_DIGI_RESULT_BYTES;
            
            // Aquí procesamos los datos para obtener el "nivel" de ruido
            // Se envía el resultado a la cola para que el módulo WiFi lo exporte
            float volume_db = 20 * log10(sum_sq / count); // Simplificado
            xQueueSend(data_queue, &volume_db, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}