// serial_manager.c
#include "serial_manager.h" // Puedes renombrar este archivo a serial_manager.h si lo prefieres
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SERIAL_MOD";

// --- CONFIGURACIÓN DEL PUERTO SERIE (UART) ---
#define SERIAL_UART_NUM       UART_NUM_0      // Cambia a UART_NUM_1 o 2 si usas pines externos
#define SERIAL_BAUD_RATE      115200          // Velocidad estándar de transmisión
#define SERIAL_TX_PIN         UART_PIN_NO_CHANGE // Si usas UART_0, usa los pines USB por defecto
#define SERIAL_RX_PIN         UART_PIN_NO_CHANGE 
#define BUF_SIZE              1024

// === FUNCIÓN INICIALIZACIÓN DEL PUERTO SERIE ===
void serial_init(void) {
    // Mantenemos el nombre de la función "wifi_init" para que no tengas que cambiar
    // tus llamadas en el main.c, pero ahora inicializa la UART.
    
    uart_config_t uart_config = {
        .baud_rate = SERIAL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Configurar parámetros de la UART
    ESP_ERROR_CHECK(uart_param_config(SERIAL_UART_NUM, &uart_config));

    // Configurar los pines (al dejar UART_PIN_NO_CHANGE usa el puerto de programación USB)
    ESP_ERROR_CHECK(uart_set_pin(SERIAL_UART_NUM, SERIAL_TX_PIN, SERIAL_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Instalar el driver de UART (sin colas de recepción porque solo vamos a transmitir)
    ESP_ERROR_CHECK(uart_driver_install(SERIAL_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "Puerto Serie (UART) inicializado correctamente a %d baudios.", SERIAL_BAUD_RATE);
}

// === TAREA EN SEGUNDO PLANO DE ENVÍO POR SERIE ===
void serial_task(void *pvParameters) {
    QueueHandle_t data_queue = (QueueHandle_t)pvParameters;
    float db_received;
    char data_str[32]; // Ampliado un poco para incluir el salto de línea de forma segura

    while (1) {
        // Bloqueo pasivo: Esperamos a que la tarea de audio procese un nuevo valor
        if (xQueueReceive(data_queue, &db_received, portMAX_DELAY)) {
            
            // Formateamos el string con un salto de línea (\r\n) para que sea fácil de leer
            // en un monitor serie, Serial Plotter, Arduino, Python, etc.
            int len = snprintf(data_str, sizeof(data_str), "%.2f\r\n", db_received);
            
            if (len > 0) {
                // Enviamos los datos directamente por el puerto serie de forma bloqueante inmediata
                int bytes_sent = uart_write_bytes(SERIAL_UART_NUM, data_str, len);
                
                if (bytes_sent < 0) {
                    ESP_LOGE(TAG, "Fallo al escribir en el puerto serie");
                }
            }
            
            // --- CONTROL DE FLUJO / VELOCIDAD ---
            // El puerto serie a 115200 es rápido, pero si la tarea de audio satura la cola,
            // descomenta esta línea para limitar la salida a 40 envíos por segundo.
            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }
}