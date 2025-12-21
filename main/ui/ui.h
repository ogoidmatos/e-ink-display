#ifndef UI_H
#define UI_H

// System includes
#include <stdint.h> 

#define BLACK 0x00
#define WHITE 0xFF
#define MID_GRAY 0x8C

#define TEMPERATURE 22

#define LOG_TAG_UI "UI"

#define LOCATION_DONE_BIT (1 << 0)
#define CURRENT_WEATHER_DONE_BIT (1 << 1)

typedef struct current_weather {
    char description[32];
    char weather_code[32];
    float temperature_c;
    float feels_like_temperature_c;
    int humidity;
    int uv_index;
    float max_temperature_c;
    float min_temperature_c;
    int is_day_time;
    int wind_speed_kph;
    int rain_chance;
} current_weather_t;

uint8_t init_ui();

uint8_t populate_base_ui();

uint8_t populate_weather_tab_ui();

uint8_t write_location_ui(const char* city, const char* country_code);
    
uint8_t refresh_weather_tab_ui();

uint8_t write_current_weather_ui(const current_weather_t* weather);

#endif // UI_H