// Own includes
#include "json_parser.h"

// ESP includes
#include "esp_log.h"

// Utils includes
#include "timezone_manager.h"
#include <string.h>

void parse_weather_json(cJSON* json, current_weather_t* weather)
{
	// Parse JSON data for weather
	weather->is_day_time = cJSON_GetObjectItem(json, "isDayTime")->valueint ? true : false;

	cJSON* weatherCondition = cJSON_GetObjectItem(json, "weatherCondition");
	const char* desc =
	  cJSON_GetObjectItem(cJSON_GetObjectItem(weatherCondition, "description"), "text")
		->valuestring;
	strncpy(weather->description, desc, sizeof(weather->description) - 1);

	const char* code = cJSON_GetObjectItem(weatherCondition, "type")->valuestring;
	strncpy(weather->weather_code, code, sizeof(weather->weather_code) - 1);

	weather->temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(json, "temperature"), "degrees")->valuedouble;
	weather->feels_like_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(json, "feelsLikeTemperature"), "degrees")
		->valuedouble;
	weather->humidity = cJSON_GetObjectItem(json, "relativeHumidity")->valueint;
	weather->uv_index = cJSON_GetObjectItem(json, "uvIndex")->valueint;

	cJSON* current_conditions = cJSON_GetObjectItem(json, "currentConditionsHistory");
	weather->max_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(current_conditions, "maxTemperature"),
								 "degrees")
		->valuedouble;
	weather->min_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(current_conditions, "minTemperature"),
								 "degrees")
		->valuedouble;

	weather->wind_speed_kph =
	  cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "wind"), "speed"), "value")
		->valueint;

	weather->rain_chance =
	  cJSON_GetObjectItem(
		cJSON_GetObjectItem(cJSON_GetObjectItem(json, "precipitation"), "probability"), "percent")
		->valueint;
}

void parse_forecast_json(cJSON* json, forecast_weather_t* forecast_array, size_t array_size)
{
	// Parse JSON data for forecast
	// today is day 0, tomorrow is day 1, day after tomorrow is day 2
	cJSON* forecastDays = cJSON_GetObjectItem(json, "forecastDays");
	for (size_t i = 0; i < array_size; i++) {
		cJSON* day = cJSON_GetArrayItem(forecastDays, i);
		if (i == 0) {
			char* time_buffer =
			  cJSON_GetObjectItem(cJSON_GetObjectItem(day, "sunEvents"), "sunriseTime")
				->valuestring;
			char* timezone =
			  cJSON_GetObjectItem(cJSON_GetObjectItem(json, "timeZone"), "id")->valuestring;
			convert_time_to_timezone(timezone, time_buffer, forecast_array[i].sunrise_time);
			time_buffer =
			  cJSON_GetObjectItem(cJSON_GetObjectItem(day, "sunEvents"), "sunsetTime")->valuestring;
			convert_time_to_timezone(timezone, time_buffer, forecast_array[i].sunset_time);
			continue; // skip today, we only want the sunrise/sunset times for today
		}

		forecast_array[i].date.year =
		  cJSON_GetObjectItem(cJSON_GetObjectItem(day, "displayDate"), "year")->valueint;
		forecast_array[i].date.month =
		  cJSON_GetObjectItem(cJSON_GetObjectItem(day, "displayDate"), "month")->valueint;
		forecast_array[i].date.day =
		  cJSON_GetObjectItem(cJSON_GetObjectItem(day, "displayDate"), "day")->valueint;

		forecast_array[i].max_temperature_c =
		  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(day, "maxTemperature"), "degrees")
			->valuedouble;
		forecast_array[i].min_temperature_c =
		  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(day, "minTemperature"), "degrees")
			->valuedouble;

		cJSON* daytimeForecast = cJSON_GetObjectItem(day, "daytimeForecast");
		cJSON* weatherCondition = cJSON_GetObjectItem(daytimeForecast, "weatherCondition");
		const char* desc =
		  cJSON_GetObjectItem(cJSON_GetObjectItem(weatherCondition, "description"), "text")
			->valuestring;
		strncpy(forecast_array[i].description, desc, sizeof(forecast_array[i].description) - 1);

		const char* code = cJSON_GetObjectItem(weatherCondition, "type")->valuestring;
		strncpy(forecast_array[i].weather_code, code, sizeof(forecast_array[i].weather_code) - 1);

		forecast_array[i].rain_chance =
		  cJSON_GetObjectItem(
			cJSON_GetObjectItem(cJSON_GetObjectItem(daytimeForecast, "precipitation"),
								"probability"),
			"percent")
			->valueint;
	}
}

int parse_events_json(cJSON* json, calendar_event_t* events)
{
	cJSON* items = cJSON_GetObjectItem(json, "items");
	if (items == NULL) {
		ESP_LOGE(LOG_TAG_JSON_PARSER, "Error parsing events JSON. No events found.");
		return -1;
	}
	const int length = cJSON_GetArraySize(items);
	if (length == 0) {
		return 0; // no events today
	}

	for (int i = 0; i < length && i < MAX_CALENDAR_EVENTS; i++) {
		cJSON* item = cJSON_GetArrayItem(items, i);

		const char* summary = cJSON_GetObjectItem(item, "summary")->valuestring;
		strlcpy(events[i].summary, summary, sizeof(events[i].summary) - 1);
		if (strlen(summary) > 40) {
			events[i].summary[38] = '.';
			events[i].summary[39] = '.';
			events[i].summary[40] = '.';
			events[i].summary[41] = '\0';
		}

		cJSON* start = cJSON_GetObjectItem(item, "start");
		cJSON* date_time = cJSON_GetObjectItem(start, "dateTime");
		// if date time is null, it means it is an all day event
		if (date_time == NULL) {
			events[i].is_all_day = true;
			continue;
		}
		const char* start_time = cJSON_GetObjectItem(start, "dateTime")->valuestring;
		const char* timezone = cJSON_GetObjectItem(start, "timeZone")->valuestring;
		// need to ponder whether it is best to convert to the timezone of the event
		// or the timezone of the device. They should be the same in most cases?
		convert_time_to_timezone(timezone, start_time, events[i].start_time);

		cJSON* end = cJSON_GetObjectItem(item, "end");
		const char* end_time = cJSON_GetObjectItem(end, "dateTime")->valuestring;
		time_difference(start_time, end_time, events[i].duration);
	}

	return length;
}

size_t unescape_c_sequences(char* buffer, size_t len)
{
	size_t read_idx = 0;
	size_t write_idx = 0;

	while (read_idx < len && buffer[read_idx] != '\0') {
		if (buffer[read_idx] == '\\' && (read_idx + 1) < len) {
			char next = buffer[read_idx + 1];
			switch (next) {
				case 'n':
					buffer[write_idx++] = '\n';
					read_idx += 2;
					continue;
				case 'r':
					buffer[write_idx++] = '\r';
					read_idx += 2;
					continue;
				case 't':
					buffer[write_idx++] = '\t';
					read_idx += 2;
					continue;
				case '\\':
					buffer[write_idx++] = '\\';
					read_idx += 2;
					continue;
				case '"':
					buffer[write_idx++] = '"';
					read_idx += 2;
					continue;
				default:
					break;
			}
		}
		buffer[write_idx++] = buffer[read_idx++];
	}

	buffer[write_idx] = '\0';
	return write_idx;
}