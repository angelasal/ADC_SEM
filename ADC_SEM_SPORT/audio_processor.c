// audio_processor.c
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h" // Librería de procesamiento de señales de Espressif
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>

// Definimos la frecuencia de muestreo (Fs).
#define SAMPLE_FREQ_HZ 16000

// CAMBIO CRÍTICO PARA LA FFT: Queremos exactamente 1024 muestras reales de audio.
#define MAX_SAMPLES    1024  

// En el ESP32-S3, cada muestra procesada por el DMA ocupa 4 bytes (SOC_ADC_DIGI_RESULT_BYTES).
// Por tanto, el tamaño del buffer en bytes será: 1024 * 4 = 4096 bytes.
#define READ_LEN       (MAX_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES) 

static adc_continuous_handle_t adc_handle = NULL;

// Función para inicializar el ADC en modo continuo (DMA)
void audio_init(void) {
    
    // 1. AJUSTE DE BUFFER: Configuración segura para evitar desbordamientos del DMA.
    // max_store_buf_size debe ser mayor que (conv_frame_size * 2 + 4). Usamos el doble (8KB).
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = READ_LEN * 2, 
        .conv_frame_size = READ_LEN,        // 4096 bytes (Equivale exactamente a 1024 muestras)
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    // Definimos cómo va a leer: Frecuencia de 16kHz y modo de una sola unidad (ADC1).
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    };
    
    // 2. AJUSTE CRÍTICO (Soluciona el reinicio RTC_SW_CPU_RST): 
    // Añadimos 'static' para que la estructura no se destruya al salir de esta función
    // y el HAL no intente leer basura de la pila de memoria.
    static adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,               // Capta hasta ~3.1V
        .channel = ADC_CHANNEL_2 & 0x7,         // GPIO 3 en ESP32-S3
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH, // 12 bits de resolución
    };
    
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern; 
    
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
}

// Tarea que calcula el volumen promedio (RMS) para detectar ruido
void audio_task(void *pvParameters) {
    // CAMBIO CRÍTICO: Sacamos 'result' de la pila haciéndolo 'static'. 
    // Evita consumir 4KB de stack en cada iteración del bucle.
    static uint8_t result[READ_LEN]; 
    uint32_t ret_num = 0;
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters; // Cola para enviar datos al WiFi

    adc_continuous_start(adc_handle); // Arrancamos el motor de captura

    while (1) {
        // Leemos el bloque completo de datos (4096 bytes). 
        if (adc_continuous_read(adc_handle, result, READ_LEN, &ret_num, pdMS_TO_TICKS(50)) == ESP_OK) {
            
            // CAMBIO CRÍTICO: 'static' evita reservar otros 4KB en la pila de la tarea.
            // Protege la memoria del sistema operativo y evita corromper el heap del WiFi.
            static adc_continuous_data_t parsed_data[MAX_SAMPLES];
            uint32_t num_parsed = 0;
            
            // TRADUCCIÓN: Pasamos los bytes crudos a datos legibles
            adc_continuous_parse_data(adc_handle, result, ret_num, parsed_data, &num_parsed);

            float sum_sq = 0;
            
            // CÁLCULO RMS (num_parsed será igual a 1024 si el buffer se llenó por completo)
            for (int i = 0; i < num_parsed; i++) {
                int raw_val = parsed_data[i].raw_data;
                float ac_val = raw_val - 2048.0f; // Quitamos el offset de continua (silencio)
                sum_sq += (ac_val * ac_val);
            }
            
            float rms = 0;
            if (num_parsed > 0) {
                rms = sqrt(sum_sq / num_parsed);
            }
            
            // PROTECCIÓN MATEMÁTICA: Evitamos el log10(0)
            float volume_db = 0;
            if (rms > 1.0f) { 
                volume_db = 20 * log10(rms); 
            }
            printf("RMS: %.2f, dB: %.2f\n", rms, volume_db);
            xQueueSend(data_queue, &volume_db, portMAX_DELAY);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void procesar_fourier(float* muestras, int n) {
    // 1. Inicializamos las tablas de la FFT (admite hasta 1024 puntos)
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, 1024);
    if (ret != ESP_OK) {
        return;
    }
    
    // 2. CAMBIO CRÍTICO: Hacer 'static' el array de la ventana de Hann. 
    // Al ser de tamaño variable (n), declarar 'float ventana[n]' creaba un array dinámico 
    // en la pila (VLA) de 4KB muy propenso a desbordar el stack si la tarea llamante era pequeña.
    static float ventana[MAX_SAMPLES];
    dsps_wind_hann_f32(ventana, n);
    for (int i = 0; i < n; i++) {
        // Multiplicamos solo la parte REAL de tus muestras complejas
        muestras[i * 2] *= ventana[i];
    }

    // 3. Ejecutamos la FFT Radix-2 Compleja
    dsps_fft2r_fc32(muestras, n);
    
    // 4. Ordenamos los resultados (Bit-reversal)
    dsps_bit_rev_fc32(muestras, n);
}