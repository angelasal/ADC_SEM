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

#define MIC_PIN         GPIO_NUM_4
#define MIC_ADC_UNIT    ADC_UNIT_1    // Obligatorio usar ADC 1 para no pisar el Wi-Fi
#define MIC_ADC_CHANNEL ADC_CHANNEL_3 // El canal 3 del ADC1 corresponde al GPIO4

static adc_continuous_handle_t adc_handle = NULL;

// Función para inicializar el ADC en modo continuo (DMA)
void audio_init(void) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_digi_pattern_config_t adc_pattern = {0};
    adc_pattern.atten = ADC_ATTEN_DB_12; 
    adc_pattern.channel = MIC_ADC_CHANNEL; // Usamos el canal 3
    adc_pattern.unit = MIC_ADC_UNIT;       // Usamos la unidad 1
    adc_pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1, // Solo la unidad 1 configurada
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    
    dig_cfg.adc_pattern = &adc_pattern;
    dig_cfg.pattern_num = 1;
    
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

// Tarea que calcula el volumen promedio (RMS) para detectar ruido
void audio_task(void *pvParameters) 
{
    // BARR-C: Variables estáticas e inicializadas al principio
    static uint8_t result[READ_LEN]; 
    uint32_t ret_num = 0;
    float nivel_confianza_ia = 0.0f;
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;

    if (data_queue == NULL) 
    {
        return; // Salida segura si la cola no se pasó correctamente
    }

    for (;;) 
    {
        // 1. Aquí leerías los datos del ADC continuo (simulado o real)
        // adc_continuous_read(adc_handle, result, READ_LEN, &ret_num, portMAX_DELAY);

        // 2. Procesamiento de la señal (Inteligencia Artificial / FFT)
        // procesar_fourier((float*)result, MAX_SAMPLES);
        
        // Simulamos que la IA ha detectado un llanto con un 85.5% de seguridad
        nivel_confianza_ia = 85.5f; 

        // 3. Enviar el resultado a la cola
        // xQueueSend devuelve pdTRUE si tiene éxito o falso si la cola está llena [3]
        if (xQueueSend(data_queue, &nivel_confianza_ia, 0) != pdTRUE) 
        {
            // BARR-C: Controlar el error si la cola se satura
            // ESP_LOGW("AUDIO", "Advertencia: La cola está llena, alerta descartada.");
        }

        // Un pequeño retardo para ceder la CPU si no estamos bloqueando en la lectura del ADC
        vTaskDelay(100 / portTICK_PERIOD_MS); 
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