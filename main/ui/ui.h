#ifndef UI_H
#define UI_H

// System includes
#include <stdint.h> 

// EPD driver includes
#include "epd_highlevel.h"

#define BLACK 0x00
#define WHITE 0xFF
#define MID_GRAY 0x8C

#define TEMPERATURE 22

#define LOG_TAG_UI "UI"

uint8_t init_ui();

uint8_t populate_base_ui();

uint8_t populate_weather_tab_ui();

uint8_t write_location_ui(const char* city, const char* country_code);

#endif // UI_H