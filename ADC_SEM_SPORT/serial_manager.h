// serial_manager.h
#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void serial_init(void);
void serial_task(void *pvParameters);

#endif // SERIAL_MANAGER_H