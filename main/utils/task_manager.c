// System includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ESP includes
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Own includes
#include "json_parser.h"
#include "network_manager.h"
#include "task_manager.h"
#include "timezone_manager.h"
#include "ui/ui.h"
#include "utils/jwt_manager.h"

static EventGroupHandle_t ui_cycle_group;
static location_t cached_location;
static struct tm current_time;
static char current_timezone[32];
static SemaphoreHandle_t http_mutex;

static void location_task(void* args)
{
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "UI cycle event group is NULL.");
		return;
	}

	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	uint8_t err = https_get_request("http://ip-api.com/json", http_output_buffer, NULL);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error performing HTTPS GET request.");
		free(http_output_buffer);
		return;
	}
	// write buffer into JSON object and free buffer
	cJSON* json = cJSON_Parse(http_output_buffer);
	free(http_output_buffer);

	if (json == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
		return;
	}

	const float latitude = (float)cJSON_GetObjectItem(json, "lat")->valuedouble;
	const float longitude = (float)cJSON_GetObjectItem(json, "lon")->valuedouble;
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Latitude: %f, Longitude: %f", latitude, longitude);

	// pass latitude and longitude to static variable so that the weather tasks run
	// after the location is fetched
	cached_location = (location_t){ latitude, longitude };

	const char* city = cJSON_GetObjectItem(json, "city")->valuestring;
	const char* country_code = cJSON_GetObjectItem(json, "countryCode")->valuestring;
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "City: %s, Country Code: %s", city, country_code);

	err = write_location_ui(city, country_code);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing location to UI.");
	}

	strcpy(current_timezone, cJSON_GetObjectItem(json, "timezone")->valuestring);

	// use the local timezone given by the ip api to convert current time to local time
	convert_time_to_local(current_timezone, time(NULL), &current_time);

	// write date to UI
	err = write_date_ui(current_time.tm_year + 1900, current_time.tm_mon, current_time.tm_mday);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing date to UI.");
	}

	// format time to HH:MM string and then print it to UI
	char time_string[16];
	err = tm_to_hour_min(&current_time, time_string);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error converting time to string.");
	}
	err = write_last_updated_ui(time_string);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing last updated to UI.");
	}

	// signal location done
	xEventGroupSetBits(ui_cycle_group, LOCATION_DONE_BIT);

	cJSON_Delete(json);

	vTaskDelete(NULL);
}

static void current_weather_task(void* args)
{
	// wait on bits to receive location from ip api if location is dynamic
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "UI cycle event group is NULL.");
		return;
	}
	xEventGroupWaitBits(ui_cycle_group,
						LOCATION_DONE_BIT,
						pdFALSE, // do not clear bits
						pdTRUE,	 // wait for all bits
						portMAX_DELAY);

	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	char url[400];
	sprintf(url,
			"https://weather.googleapis.com/v1/currentConditions:lookup"
			"?key=%s&location.latitude=%f&location.longitude=%f&prettyPrint=false&fields=isDaytime,"
			"weatherCondition(description,type),temperature,feelsLikeTemperature,relativeHumidity,"
			"uvIndex,precipitation(probability),wind(speed),currentConditionsHistory("
			"maxTemperature,minTemperature)",
			GOOGLE_API_KEY,
			cached_location.latitude,
			cached_location.longitude);

	xSemaphoreTake(http_mutex, portMAX_DELAY);
	uint8_t err = https_get_request(url, http_output_buffer, NULL);
	xSemaphoreGive(http_mutex);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error performing HTTPS GET request.");
		free(http_output_buffer);
		return;
	}

	// write buffer into JSON object and free buffer
	cJSON* json = cJSON_Parse(http_output_buffer);

	if (json == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Output buffer: %s", http_output_buffer);
		free(http_output_buffer);
		return;
	}
	free(http_output_buffer);

	// Create and populate the weather struct
	current_weather_t weather = { 0 };
	parse_weather_json(json, &weather);

	ESP_LOGD(LOG_TAG_TASK_MANAGER,
			 "Weather: %s, Code: %s, Temp: %.2fC, Feels like: %.2fC, Humidity: %d%%, UV Index: %d, "
			 "Max Temp: %.2fC, Min Temp: %.2fC, Is Day Time: %d, Wind: %d kph, Rain Chance: %d%%",
			 weather.description,
			 weather.weather_code,
			 weather.temperature_c,
			 weather.feels_like_temperature_c,
			 weather.humidity,
			 weather.uv_index,
			 weather.max_temperature_c,
			 weather.min_temperature_c,
			 weather.is_day_time,
			 weather.wind_speed_kph,
			 weather.rain_chance);

	err = write_current_weather_ui(&weather);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing current weather to UI.");
	}
	// signal current weather done
	xEventGroupSetBits(ui_cycle_group, CURRENT_WEATHER_DONE_BIT);

	cJSON_Delete(json);
	vTaskDelete(NULL);
}

static void refresh_task(void* args)
{
	// wait on bits to receive location and current weather
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "UI cycle event group is NULL.");
		return;
	}
	xEventGroupWaitBits(ui_cycle_group,
						LOCATION_DONE_BIT | CURRENT_WEATHER_DONE_BIT | FORECAST_WEATHER_DONE_BIT |
						  EVENTS_DONE_BIT,
						pdTRUE, // clear bits
						pdTRUE, // wait for all bits
						portMAX_DELAY);
	uint8_t err = refresh_screen_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error refreshing weather tab UI.");
	}

	// after updating the screen, send the device to deep sleep
	disconnect_wifi();
	esp_deep_sleep_start();
	vTaskDelete(NULL);
}

static void forecast_weather_task(void* args)
{
	// wait on bits to receive location from ip api if location is dynamic
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "UI cycle event group is NULL.");
		return;
	}
	xEventGroupWaitBits(ui_cycle_group,
						LOCATION_DONE_BIT,
						pdFALSE, // do not clear bits
						pdTRUE,	 // wait for all bits
						portMAX_DELAY);

	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	char url[360];
	sprintf(url,
			"https://weather.googleapis.com/v1/forecast/days:lookup"
			"?key=%s&location.latitude=%f&location.longitude=%f&days=3&prettyPrint=false&fields="
			"timeZone,forecastDays(displayDate,maxTemperature,minTemperature,sunEvents,"
			"daytimeForecast(weatherCondition(description,type),precipitation(probability)))",
			GOOGLE_API_KEY,
			cached_location.latitude,
			cached_location.longitude);

	xSemaphoreTake(http_mutex, portMAX_DELAY);
	uint8_t err = https_get_request(url, http_output_buffer, NULL);
	xSemaphoreGive(http_mutex);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error performing HTTPS GET request.");
		free(http_output_buffer);
		return;
	}
	// write buffer into JSON object and free buffer
	cJSON* json = cJSON_Parse(http_output_buffer);

	if (json == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Output buffer: %s", http_output_buffer);
		free(http_output_buffer);
		return;
	}
	free(http_output_buffer);

	// Create and populate the forecast array
	forecast_weather_t forecast_array[3] = { 0 };
	parse_forecast_json(json, forecast_array, 3);

	err = write_forecast_ui(forecast_array);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing forecast to UI.");
	}

	// signal forecast weather done
	xEventGroupSetBits(ui_cycle_group, FORECAST_WEATHER_DONE_BIT);

	cJSON_Delete(json);
	vTaskDelete(NULL);
}

static void calendar_task(void* args)
{
	// wait on location task to set the current timezone and localtime
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "UI cycle event group is NULL.");
		return;
	}
	xEventGroupWaitBits(ui_cycle_group,
						LOCATION_DONE_BIT,
						pdFALSE, // do not clear bits
						pdTRUE,	 // wait for all bits
						portMAX_DELAY);

	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	// get private key from nvs, stored in previous build
	nvs_handle_t nvs_handle;
	ESP_ERROR_CHECK(nvs_open("storage", NVS_READONLY, &nvs_handle));
	size_t required_size;
	nvs_get_str(nvs_handle, "priv_key", NULL, &required_size);
	char* private_key = malloc(required_size);
	nvs_get_str(nvs_handle, "priv_key", private_key, &required_size);

	// create JWT with private key
	char* jwt = createGCPJWT(CLIENT_EMAIL, (uint8_t*)private_key, strlen(private_key) + 1);

	// erase private key from memory
	memset(private_key, 0, required_size);
	free(private_key);

	ESP_LOGD(LOG_TAG_TASK_MANAGER, "JWT: %s", jwt);

	xSemaphoreTake(http_mutex, portMAX_DELAY);
	uint8_t err =
	  https_gcp_auth_post_request("https://oauth2.googleapis.com/token", jwt, http_output_buffer);
	xSemaphoreGive(http_mutex);
	free(jwt);

	cJSON* token_json = cJSON_Parse(http_output_buffer);
	if (token_json == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Output buffer: %s", http_output_buffer);
		free(http_output_buffer);
		return;
	}
	const char* bearer_token = cJSON_GetObjectItem(token_json, "access_token")->valuestring;
	if (bearer_token == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error getting bearer token from JSON response.");
		free(http_output_buffer);
		cJSON_Delete(token_json);
		return;
	}

	char url[252];
	char date_buffer[12];
	strftime(date_buffer, 12, "%Y-%m-%d", &current_time);
	sprintf(url,
			"https://content.googleapis.com/calendar/v3/calendars/%s/events?"
			"singleEvents=true&timeMin=%sT00:00:00Z&timeMax=%sT23:59:00Z&orderBy=startTime&"
			"fields=items(summary,start,end)",
			CALENDAR_TARGET,
			date_buffer,
			date_buffer);

	ESP_LOGD(LOG_TAG_TASK_MANAGER, "%s", url);

	xSemaphoreTake(http_mutex, portMAX_DELAY);
	err = https_get_request(url, http_output_buffer, bearer_token);
	xSemaphoreGive(http_mutex);

	cJSON_Delete(token_json);

	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error performing HTTPS GET request.");
		free(http_output_buffer);
		return;
	}
	// write buffer into JSON object and free buffer
	cJSON* json = cJSON_Parse(http_output_buffer);
	if (json == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Output buffer: %s", http_output_buffer);
		free(http_output_buffer);
		return;
	}

	calendar_event_t events[MAX_CALENDAR_EVENTS] = { 0 };
	int num_events = parse_events_json(json, events);
	cJSON_Delete(json);

	if (num_events < 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing calendar events JSON.");
		free(http_output_buffer);
		return;
	} else if (num_events > 0) {
		// write events to UI
		err = write_calendar_events_ui(events, num_events);
		if (err != 0) {
			ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing calendar events to UI.");
		}
	} else {
		// no events, get random fact of the day and write to UI
		strcpy(url, "https://uselessfacts.jsph.pl/random.json");
		xSemaphoreTake(http_mutex, portMAX_DELAY);
		uint8_t err = https_get_request(url, http_output_buffer, NULL);
		xSemaphoreGive(http_mutex);
		if (err != 0) {
			ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error performing HTTPS GET request.");
			free(http_output_buffer);
			return;
		}
		cJSON* fact_json = cJSON_Parse(http_output_buffer);
		if (fact_json == NULL) {
			ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error parsing JSON response.");
			ESP_LOGE(LOG_TAG_TASK_MANAGER, "Output buffer: %s", http_output_buffer);
			free(http_output_buffer);
			return;
		}
		const char* fact = cJSON_GetObjectItem(fact_json, "text")->valuestring;
		err = write_fact_ui(fact);
		if (err != 0) {
			ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing fact to UI.");
		}
		cJSON_Delete(fact_json);
	}

	// signal calendar events tab done
	xEventGroupSetBits(ui_cycle_group, EVENTS_DONE_BIT);

	free(http_output_buffer);

	vTaskDelete(NULL);
}

// --------------------- Task start functions ---------------- //

uint8_t start_location_task()
{
	// Check if we are using static location
	// If dynamic location is disabled, set latitude and longitude to the configured values
	// and do not start the location task
#if !defined(CONFIG_USE_DYNAMIC_LOCATION) || (CONFIG_USE_DYNAMIC_LOCATION == 0)
	latitude = atof(LATITUDE);
	longitude = atof(LONGITUDE);
	// fix is required, when using static location, the UI is not updated with the location
	// TODO: when fetch weather, also get location to print in the UI
	ESP_LOGD(LOG_TAG_TASK_MANAGER,
			 "Using static location. Latitude: %f, Longitude: %f",
			 latitude,
			 longitude);
	return 0;
#else
	uint8_t err = xTaskCreate(location_task, "location_task", 4096, NULL, 6, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating location task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Location task created.");
	return 0;
#endif
}

uint8_t start_weather_tasks()
{
	http_mutex = xSemaphoreCreateMutex();
	if (http_mutex == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating HTTP mutex.");
		return 1;
	}
	uint8_t err = xTaskCreate(current_weather_task, "current_weather_task", 4096, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating current weather task.");
		return 1;
	}
	err = xTaskCreate(forecast_weather_task, "forecast_weather_task", 4096, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating forecast weather task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Weather tasks created.");
	return 0;
}

uint8_t start_refresh_task()
{
	ui_cycle_group = xEventGroupCreate();
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_UI, "Error creating UI cycle event group.");
		return 1;
	}

	uint8_t err = xTaskCreate(refresh_task, "refresh_task", 4096, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating refresh task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Refresh task created.");
	return 0;
}

uint8_t start_calendar_task()
{
	uint8_t err = xTaskCreate(calendar_task, "calendar_task", 8192, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating calendar task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Calendar task created.");
	return 0;
}