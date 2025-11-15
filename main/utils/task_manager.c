// System includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ESP includes
#include "cJSON.h"
#include "esp_log.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Own includes
#include "network_manager.h"
#include "task_manager.h"
#include "ui/ui.h"

static QueueHandle_t location_queue = NULL;

static void location_task(void* args)
{
	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	uint8_t err = https_get_request("http://ip-api.com/json", http_output_buffer);
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

	// pass latitude and longitude in a queue to the weather task to ensure that it runs
	// after the location is fetched
	location_t current_location = { latitude, longitude };
	if (xQueueSend(location_queue, (void*)&current_location, pdMS_TO_TICKS(1000)) != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error sending location to queue.");
	}

	const char* city = cJSON_GetObjectItem(json, "city")->valuestring;
	const char* country_code = cJSON_GetObjectItem(json, "countryCode")->valuestring;
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "City: %s, Country Code: %s", city, country_code);

	err = write_location_ui(city, country_code);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing location to UI.");
	}

	cJSON_Delete(json);

	vTaskDelete(NULL);
}

static void current_weather_task(void* args)
{
	// wait on queue to receive location from ip api if location is dynamic
	if (location_queue == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Location queue is NULL.");
		return;
	}
	location_t current_location;
	if (xQueueReceive(location_queue, &current_location, portMAX_DELAY) != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error receiving location from queue.");
		return;
	}

	char* http_output_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
	if (http_output_buffer == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error allocating memory for HTTP output buffer.");
		free(http_output_buffer);
		return;
	}

	char url[256];
	sprintf(url,
			"https://weather.googleapis.com/v1/"
			"currentConditions:lookup?key=%s&location.latitude=%f&location.longitude=%f",
			WEATHER_API_KEY,
			current_location.latitude,
			current_location.longitude);

	uint8_t err = https_get_request(url, http_output_buffer);
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

	// Parse JSON data for weather
	const int is_day_time = cJSON_GetObjectItem(json, "isDayTime")->valueint;
	const cJSON* weatherCondition = cJSON_GetObjectItem(json, "weatherCondition");
	const char* description =
	  cJSON_GetObjectItem(cJSON_GetObjectItem(weatherCondition, "description"), "text")
		->valuestring;
	const char* weather_code = cJSON_GetObjectItem(weatherCondition, "type")->valuestring;
	const float temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(json, "temperature"), "degrees")->valuedouble;
	const float feels_like_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(json, "feelsLikeTemperature"), "degrees")
		->valuedouble;
	const int humidity = cJSON_GetObjectItem(json, "relativeHumidity")->valueint;
	const int uv_index = cJSON_GetObjectItem(json, "uvIndex")->valueint;
	const cJSON* current_conditions = cJSON_GetObjectItem(json, "currentConditionsHistory");
	const float max_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(current_conditions, "maxTemperature"),
								 "degrees")
		->valuedouble;
	const float min_temperature_c =
	  (float)cJSON_GetObjectItem(cJSON_GetObjectItem(current_conditions, "minTemperature"),
								 "degrees")
		->valuedouble;
	const int wind_speed_kph =
	  cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "wind"), "speed"), "value")
		->valueint;

	ESP_LOGD(LOG_TAG_TASK_MANAGER,
			 "Weather: %s, Code: %s, Temp: %.2fC, Feels like: %.2fC, Humidity: %d%%, UV Index: %d, "
			 "Max Temp: %.2fC, Min Temp: %.2fC, Is Day Time: %d",
			 description,
			 weather_code,
			 temperature_c,
			 feels_like_temperature_c,
			 humidity,
			 uv_index,
			 max_temperature_c,
			 min_temperature_c,
			 is_day_time,
			 wind_speed_kph);

	cJSON_Delete(json);
	vTaskDelete(NULL);
}

static void refresh_task(void* args)
{
	uint8_t err = refresh_weather_tab_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error refreshing weather tab UI.");
	}
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
	location_queue = xQueueCreate(2, sizeof(location_t));
	if (location_queue == NULL) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating location queue.");
		return 1;
	}

	uint8_t err = xTaskCreate(location_task, "location_task", 4096, NULL, 5, NULL);
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
	uint8_t err = xTaskCreate(current_weather_task, "current_weather_task", 4096, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating weather task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Weather task created.");
	return 0;
}

uint8_t start_refresh_task()
{
	uint8_t err = xTaskCreate(refresh_task, "refresh_task", 4096, NULL, 5, NULL);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating refresh task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Refresh task created.");
	return 0;
}