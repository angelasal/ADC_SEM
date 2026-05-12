#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>            
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "PROYECTO_ADC_S3";

/* --- CONFIGURACIÓN TEOREMA NYQUIST-SHANNON --- 
 * Justificación: Para evitar el aliasing, muestreamos a una frecuencia (Fs) 
 * mayor al doble de la frecuencia máxima de la señal (Fmax).
 */
#define F_MAX_SENAL        0.5f     // Suponemos que la temperatura no cambia más de 0.5 veces por segundo
#define FACTOR_NYQUIST     4        // Elegimos un factor de 4 para mayor precisión (Nyquist pide > 2)
#define SAMPLING_FREQ      (F_MAX_SENAL * FACTOR_NYQUIST) // Fs = 2 Hz
#define SAMPLE_PERIOD_MS   ((1 / SAMPLING_FREQ) * 1000)   // T = 500ms entre muestras

/* --- CONFIGURACIÓN NTC --- 
 * Datos extraídos del datasheet del sensor NTC común (MF52). Deberíamos ver cuál estamos usando en clase y ajustar estos parámetros.
 */
#define NTC_BETA           3950.0f  // Parámetro Beta: define la curvatura de la resistencia
#define NTC_R25            10000.0f // Resistencia nominal a 25°C (10k ohms)
#define R_FIXED            10000.0f // Resistencia fija del divisor de tensión (10k ohms)
#define V_REF              3300.0f  // Voltaje de alimentación del circuito en milivoltios (3.3V)

/* --- CONFIGURACIÓN HARDWARE ADC --- */
#define EXAMPLE_ADC_ATTEN  ADC_ATTEN_DB_12 // Atenuación para leer hasta ~3.1V (Rango máximo en S3)
#define CHAN_COUNT         1
adc_channel_t channels[CHAN_COUNT] = {ADC_CHANNEL_2}; // ADC1_CH2 suele ser el GPIO 3 en ESP32-S3

// Prototipos de funciones
static bool init_calibration(adc_unit_t unit, adc_channel_t channel, adc_cali_handle_t *out_handle);
float calcular_temperatura_ntc(int voltaje_mv);

void app_main(void)
{
    // --- 1. CONFIGURACIÓN DEL PERIFÉRICO (ADC ONESHOT) ---
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // --- 2. CONFIGURACIÓN DEL CANAL ---
    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Por defecto usa 12 bits (0 a 4095)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channels[0], &config));

    // --- 3. INICIALIZACIÓN DE CALIBRACIÓN ---
    // Esto compensa los errores internos del ADC para obtener milivoltios reales.
    adc_cali_handle_t cali_handle = NULL;
    bool calibrated = init_calibration(ADC_UNIT_1, channels[0], &cali_handle);

    // Variable para controlar el tiempo exacto de Nyquist
    int raw_data, voltage_mv;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    ESP_LOGI(TAG, "Iniciando muestreo a %.1f Hz...", SAMPLING_FREQ);

    while (1) {
        // A. Lectura del valor crudo (0-4095)
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channels[0], &raw_data));
        
        if (calibrated) {
            // B. Transformación de valor crudo a Milivoltios (usando curva de calibración)
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_data, &voltage_mv));
            
            // C. Procesamiento Matemático (De Voltios a Grados)
            float temp_c = calcular_temperatura_ntc(voltage_mv);
            
            // D. Salida de datos por consola
            ESP_LOGI(TAG, "Temp: %.2f °C | Voltaje: %d mV | Raw: %d", temp_c, voltage_mv, raw_data);
        }

        // --- E. CONTROL DE TIEMPO (NYQUIST) ---
        /* vTaskDelayUntil es preferible a vTaskDelay porque tiene en cuenta el tiempo
           que el procesador tardó en hacer los cálculos anteriores, asegurando que 
           el periodo de muestreo sea EXACTAMENTE 500ms (2Hz). */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

/**
 * Función: calcular_temperatura_ntc
 * --------------------------------
 * Implementa la ley de Ohm para el divisor de tensión y la ecuación de Steinhart-Hart.
 */
float calcular_temperatura_ntc(int voltaje_mv) {
    // Evitamos división por cero si el cable se suelta, así podemos reconocer si el error está en el hardware.
    if (voltaje_mv <= 0 || voltaje_mv >= V_REF) return 0.0f;

    /* 1. Cálculo de Resistencia del NTC (Regla del divisor de tensión):
       V_out = V_cc * (R_ntc / (R_fija + R_ntc))  --> Despejamos R_ntc */
    float r_ntc = R_FIXED / ((V_REF / (float)voltaje_mv) - 1.0f);

    /* 2. Ecuación simplificada de Steinhart-Hart (Modelo Beta):
       1/T = 1/T0 + 1/Beta * ln(R/R0)  [Resultado en Kelvin] */
    float temp_k = 1.0f / ((1.0f / 298.15f) + (1.0f / NTC_BETA) * log(r_ntc / NTC_R25));
    
    // 3. Conversión de Kelvin a Celsius
    return temp_k - 273.15f; 
}

/**
 * Función: init_calibration, viene dada en el template.
 * -------------------------
 * El ESP32-S3 tiene esquemas de calibración grabados de fábrica (eFuses).
 * Esta función detecta si el chip soporta "Curve Fitting" (ajuste por curva)
 * o "Line Fitting" (ajuste lineal) para que el voltaje leído sea preciso.
 */
static bool init_calibration(adc_unit_t unit, adc_channel_t channel, adc_cali_handle_t *out_handle) {
    esp_err_t ret = ESP_FAIL;
    bool success = false;

    // Intentamos el esquema de ajuste por curva (el más preciso en S3)
    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cfg = { .unit_id = unit, .chan = channel, .atten = EXAMPLE_ADC_ATTEN, .bitwidth = ADC_BITWIDTH_DEFAULT };
        ret = adc_cali_create_scheme_curve_fitting(&cfg, out_handle);
    #elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t cfg = { .unit_id = unit, .atten = EXAMPLE_ADC_ATTEN, .bitwidth = ADC_BITWIDTH_DEFAULT };
        ret = adc_cali_create_scheme_line_fitting(&cfg, out_handle);
    #endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibración inicializada correctamente.");
        success = true;
    } else {
        ESP_LOGW(TAG, "Aviso: No hay datos de calibración en eFuse, se usará valor crudo.");
    }
    return success;
}

//MONTAJE

/* 

Para usar un NTC con la S3, el reto es que la relación entre resistencia y temperatura no es una línea recta, sino una curva.

1. El Circuito (Divisor de Tensión)
El ADC no mide resistencia, mide voltaje. Por eso necesitas un divisor de tensión con una resistencia fija (preferiblemente de 10kΩ con 1% de tolerancia).

VCC: 3.3V de la ESP32-S3.

R_Fija: 10kΩ conectada a VCC.

NTC: Conectada entre el pin del ADC y GND.

Punto de medida: El pin del ADC va entre la resistencia fija y la NTC.
*/
