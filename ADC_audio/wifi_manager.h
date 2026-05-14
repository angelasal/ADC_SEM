// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * Inicializa el stack de red y conecta el ESP32-S3 a la red WiFi.
 */
void wifi_init(void);

/**
 * Tarea de FreeRTOS que espera datos de la cola y los envía a la nube.
 * @param pvParameters Puntero a la QueueHandle_t de donde leer los dB.
 */
void cloud_task(void *pvParameters);

#endif