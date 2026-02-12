#ifndef UI_H
#define UI_H

// System includes
#include <stdbool.h>
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

#define MAX_CALENDAR_EVENTS 4

typedef struct current_weather {
    float temperature_c;
    float feels_like_temperature_c;
    float max_temperature_c;
    float min_temperature_c;
    uint8_t wind_speed_kph;
    uint8_t humidity;
    uint8_t uv_index;
    uint8_t rain_chance;
    bool is_day_time;
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

typedef struct calendar_event {
    char summary[42];
    char duration[32];
    char start_time[6];
    bool is_all_day;
} calendar_event_t;

uint8_t init_ui(float battery_percentage);

uint8_t populate_base_ui(float battery_percentage);

uint8_t populate_weather_tab_ui();

uint8_t write_location_ui(const char* city, const char* country_code);
    
uint8_t refresh_screen_ui();

uint8_t write_current_weather_ui(const current_weather_t* weather);

uint8_t write_date_ui(uint16_t year, uint8_t month, uint8_t day, uint8_t day_of_week);

uint8_t write_forecast_ui(const forecast_weather_t* forecast_array);

uint8_t write_last_updated_ui(const char* time_string);

uint8_t write_fact_ui(const char* fact);

uint8_t write_calendar_events_ui(const calendar_event_t* events, int event_count);

#endif // UI_H