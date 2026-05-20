// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void wifi_init(void);
void cloud_task(void *pvParameters);

#endif // WIFI_MANAGER_H