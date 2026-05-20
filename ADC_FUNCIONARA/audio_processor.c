// audio_processor.c
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h" // Librería de procesamiento de señales de Espressif
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>

// Definimos la frecuencia de muestreo (Fs). 
// Aquí aplicamos el TEOREMA DE NYQUIST: Fs >= 2 * Fmax.
// 16000Hz nos permite capturar sonidos de hasta 8000Hz (voz y ambiente).
#define SAMPLE_FREQ_HZ 16000
#define READ_LEN 1024        // Tamaño del buffer para el procesamiento

static adc_continuous_handle_t adc_handle = NULL;

// Función para inicializar el ADC en modo continuo (DMA)
void audio_init(void) {
    // Configuramos el buffer de almacenamiento (DMA)
    // El DMA permite que el ADC guarde datos en la RAM sin usar la CPU.
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    // Definimos cómo va a leer: Frecuencia de 16kHz y modo de una sola unidad.
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    };
    
    // Configuramos el pin físico (Pattern)
    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,       // Permite leer hasta ~3.1V
        .channel = ADC_CHANNEL_2 & 0x7, // Pin del micrófono (GPIO 3 en S3)
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH, // 12 bits de resolución
    };
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
}

// Tarea que calcula el volumen promedio (RMS) para detectar ruido
void audio_task(void *pvParameters) {
    uint8_t result[READ_LEN];
    uint32_t ret_num = 0;
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters; // Cola para enviar datos al WiFi

    adc_continuous_start(adc_handle); // Arrancamos el motor de captura

    while (1) {
        // Leemos el bloque de datos que el DMA ha llenado
        if (adc_continuous_read(adc_handle, result, READ_LEN, &ret_num, 0) == ESP_OK) {
            
            // 1. TRADUCCIÓN: Pasamos los bytes crudos a datos reales del ADC
            adc_continuous_data_t parsed_data[READ_LEN / SOC_ADC_DIGI_RESULT_BYTES];
            uint32_t num_parsed = 0;
            adc_continuous_parse_data(adc_handle, result, ret_num, parsed_data, &num_parsed);

            float sum_sq = 0;
            
            // 2. CÁLCULO RMS
            for (int i = 0; i < num_parsed; i++) {
                int raw_val = parsed_data[i].raw_data;
                float ac_val = raw_val - 2048.0f; // Quitamos el nivel de silencio
                sum_sq += (ac_val * ac_val);
            }
            
            float rms = sqrt(sum_sq / num_parsed);
            
            // 3. PROTECCIÓN MATEMÁTICA: Evitamos el log10(0)
            float volume_db = 0;
            if (rms > 1.0f) { // Si hay aunque sea un mínimo de ruido
                volume_db = 20 * log10(rms); 
            }
            
            xQueueSend(data_queue, &volume_db, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Pausa para no saturar el procesador
    }
}


void procesar_fourier(float* muestras, int n) {
    // 1. Inicializamos las tablas de la FFT (admite hasta 1024 puntos)
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, 1024);
    if (ret != ESP_OK) {
        return;
    }
    
    // 2. Aplicamos la ventana de Hann
    float ventana[n];
    dsps_wind_hann_f32(ventana, n);
    for (int i = 0; i < n; i++) {
        // Multiplicamos solo la parte REAL de tus muestras complejas
        // En un array complejo: muestras[0]=R0, muestras[1]=I0, muestras[2]=R1, muestras[3]=I1...
        muestras[i * 2] *= ventana[i];
    }

    // 3. Ejecutamos la FFT Radix-2 Compleja (La que te sugiere el propio compilador)
    dsps_fft2r_fc32(muestras, n);
    
    // 4. Ordenamos los resultados (Bit-reversal)
    dsps_bit_rev_fc32(muestras, n);

}