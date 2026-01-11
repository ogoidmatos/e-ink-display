#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

// System includes
#include <stdint.h>

#define LOG_TAG_TASK_MANAGER "TASK_MANAGER"

#define CORE1 1
#define CORE0 0

typedef struct location {
    float latitude;
    float longitude;
} location_t;

#define LOCATION_DONE_BIT (1 << 0)
#define CURRENT_WEATHER_DONE_BIT (1 << 1)
#define FORECAST_WEATHER_DONE_BIT (1 << 2)

uint8_t start_location_task();
uint8_t start_weather_tasks();
uint8_t start_refresh_task();

#endif // TASK_MANAGER_H