#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "cJSON.h"

#include "ui/ui.h"

#define LOG_TAG_JSON_PARSER "JSON_PARSER"

void parse_weather_json(cJSON* json, current_weather_t* weather);
void parse_forecast_json(cJSON* json, forecast_weather_t* forecast_array, size_t array_size);
int parse_events_json(cJSON* json, calendar_event_t* events);
size_t unescape_c_sequences(char* buffer, size_t len);

#endif  // JSON_PARSER_H