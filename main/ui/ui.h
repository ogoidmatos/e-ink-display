#ifndef UI_H
#define UI_H

// System includes
#include <stdint.h> 

#define BLACK 0x00
#define WHITE 0xFF
#define MID_GRAY 0x8C

#define TEMPERATURE 22

#define LOG_TAG_UI "UI"

#define CURRENT_WEATHER_WIDGET_WIDTH 400
#define CURRENT_WEATHER_WIDGET_HEIGHT 220
#define FORECAST_WEATHER_WIDGET_WIDTH 400
#define FORECAST_WEATHER_WIDGET_HEIGHT 60

typedef struct current_weather {
    float temperature_c;
    float feels_like_temperature_c;
    float max_temperature_c;
    float min_temperature_c;
    uint8_t wind_speed_kph;
    uint8_t humidity;
    uint8_t uv_index;
    uint8_t rain_chance;
    uint8_t is_day_time;
    char weather_code[32];
    char description[32];
} current_weather_t;

typedef struct date {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} date_t;

typedef struct forecast_weather {
    float max_temperature_c;
    float min_temperature_c;
    date_t date;
    char weather_code[32];
    char description[32];
    char sunrise_time[6];
    char sunset_time[6];
    uint8_t rain_chance;
} forecast_weather_t;

uint8_t init_ui(float battery_percentage);

uint8_t populate_base_ui(float battery_percentage);

uint8_t populate_weather_tab_ui();

uint8_t write_location_ui(const char* city, const char* country_code);
    
uint8_t refresh_screen_ui();

uint8_t write_current_weather_ui(const current_weather_t* weather);

uint8_t write_date_ui(uint16_t year, uint8_t month, uint8_t day);

uint8_t write_forecast_ui(const forecast_weather_t* forecast_array);

#endif // UI_H