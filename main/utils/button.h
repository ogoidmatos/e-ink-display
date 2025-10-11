#ifndef BUTTON_H
#define BUTTON_H

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define BUTTON_PIN 21

void button_switch_context_init();

#endif // BUTTON_H