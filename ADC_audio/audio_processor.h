// audio_processor.h
#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void audio_init(void);
void audio_task(void *pvParameters);
void procesar_fourier(float* muestras, int n);

#endif