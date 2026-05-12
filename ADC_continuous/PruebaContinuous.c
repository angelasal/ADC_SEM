/*
 * PROYECTO: Monitor de Temperatura con ADC Continuo y NTC
 * DISPOSITIVO: ESP32-S3
 */

#include <string.h>
#include <stdio.h>
#include <math.h>            
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// --- CONFIGURACIÓN NTC --- 
#define NTC_BETA           3950.0f  // Parámetro Beta del sensor
#define NTC_R25            10000.0f // Resistencia nominal a 25°C (10k)
#define R_FIXED            10000.0f // Resistencia fija del divisor (10k)
#define V_REF              3300.0f  // Voltaje de alimentación (3.3V en mV)

// --- CONFIGURACIÓN HARDWARE ADC --- 
#define EXAMPLE_ADC_UNIT           ADC_UNIT_1
#define EXAMPLE_ADC_CONV_MODE      ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN          ADC_ATTEN_DB_12 // Para leer hasta ~3.1V
#define EXAMPLE_READ_LEN           256             // Tamaño del bloque de datos
#define SAMPLING_FREQ_HZ           2000            // 2kHz (Cumple Nyquist para temperatura)

static const char *TAG = "PROYECTO_ADC";
static TaskHandle_t s_task_handle;

// Prototipos de funciones
static bool init_calibration(adc_unit_t unit, adc_channel_t channel, adc_cali_handle_t *out_handle);
float calcular_temperatura_ntc(int voltaje_mv);

/**
 * Callback de Interrupción (ISR): Se ejecuta cuando el buffer del ADC está lleno.
 */
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    // Despierta a la tarea principal para procesar los datos
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

void app_main(void)
{
    s_task_handle = xTaskGetCurrentTaskHandle();

    // --- 1. CONFIGURACIÓN DEL DRIVER CONTINUO ---
    adc_continuous_handle_t handle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLING_FREQ_HZ,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
    };

    adc_digi_pattern_config_t pattern = {
        .atten = EXAMPLE_ADC_ATTEN,
        .channel = ADC_CHANNEL_2 & 0x7, // GPIO 3 en ESP32-S3
        .unit = EXAMPLE_ADC_UNIT,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    // --- 2. INICIALIZACIÓN DE CALIBRACIÓN ---
    adc_cali_handle_t cali_handle = NULL;
    bool calibrated = init_calibration(EXAMPLE_ADC_UNIT, ADC_CHANNEL_2, &cali_handle);

    // --- 3. REGISTRO DE EVENTOS Y ARRANQUE ---
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    uint8_t result[EXAMPLE_READ_LEN] = {0};
    uint32_t ret_num = 0;

    ESP_LOGI(TAG, "Leyendo temperatura...");

    while (1) {
        // Espera la notificación del Callback de que el ADC ha llenado el buffer
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0) == ESP_OK) {
            
            adc_continuous_data_t parsed_data[ret_num / SOC_ADC_DIGI_RESULT_BYTES];
            uint32_t num_parsed = 0;

            // Traducir bytes crudos a estructura de datos
            esp_err_t ret = adc_continuous_parse_data(handle, result, ret_num, parsed_data, &num_parsed);
            
            if (ret == ESP_OK) {
                for (int i = 0; i < num_parsed; i++) {
                    int voltage_mv = 0;
                    if (calibrated) {
                        // Convertir valor crudo a Milivoltios usando la calibración de fábrica
                        adc_cali_raw_to_voltage(cali_handle, parsed_data[i].raw_data, &voltage_mv);
                        
                        // Calcular temperatura final
                        float temp_c = calcular_temperatura_ntc(voltage_mv);
                        
                        ESP_LOGI(TAG, "Canal: %d | Temp: %.2f °C | Voltaje: %d mV | Raw: %"PRIu32,
                                 parsed_data[i].channel, temp_c, voltage_mv, parsed_data[i].raw_data);
                    }
                }
            }
            // Retardo para no saturar la salida del monitor serie
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/**
 * Aplica la ley de Ohm para el divisor y la ecuación de Steinhart-Hart.
 */
float calcular_temperatura_ntc(int voltaje_mv) {
    if (voltaje_mv <= 0 || voltaje_mv >= V_REF) return 0.0f;

    // 1. Cálculo de Resistencia del NTC
    float r_ntc = R_FIXED / ((V_REF / (float)voltaje_mv) - 1.0f);

    // 2. Ecuación de Steinhart-Hart (Modelo Beta)
    float temp_k = 1.0f / ((1.0f / 298.15f) + (1.0f / NTC_BETA) * log(r_ntc / NTC_R25));
    
    // 3. Conversión a Celsius
    return temp_k - 273.15f; 
}

/**
 * Configura la calibración específica para el hardware de la S3.
 */
static bool init_calibration(adc_unit_t unit, adc_channel_t channel, adc_cali_handle_t *out_handle) {
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = unit,
        .chan = channel,
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    return (adc_cali_create_scheme_curve_fitting(&cfg, out_handle) == ESP_OK);
}