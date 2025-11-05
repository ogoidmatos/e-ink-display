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
#include "freertos/task.h"

// Own includes
#include "network_manager.h"
#include "task_manager.h"
#include "ui/ui.h"

static volatile float latitude = 0.0;
static volatile float longitude = 0.0;

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

	latitude = cJSON_GetObjectItem(json, "lat")->valuedouble;
	longitude = cJSON_GetObjectItem(json, "lon")->valuedouble;
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Latitude: %f, Longitude: %f", latitude, longitude);

	char* city = cJSON_GetObjectItem(json, "city")->valuestring;
	char* country_code = cJSON_GetObjectItem(json, "countryCode")->valuestring;
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "City: %s, Country Code: %s", city, country_code);

	err = write_location_ui(city, country_code);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error writing location to UI.");
	}

	cJSON_Delete(json);

	vTaskDelete(NULL);
}

uint8_t start_location_task()
{
	uint8_t err =
	  xTaskCreatePinnedToCore(location_task, "location_task", 4096, NULL, 5, NULL, CORE1);
	if (err != pdPASS) {
		ESP_LOGE(LOG_TAG_TASK_MANAGER, "Error creating location task.");
		return 1;
	}
	ESP_LOGD(LOG_TAG_TASK_MANAGER, "Location task created.");
	return 0;
}