// Standard includes
#include <stdint.h>
#include <stdio.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP includes
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

// EPD driver includes
#include "epd_highlevel.h"

// Own includes
#include "ui/ui.h"
#include "utils/button.h"
#include "utils/network_manager.h"
#include "utils/task_manager.h"

#define LOG_TAG_MAIN "MAIN"
#define SECONDS_TO_MICROSECONDS 1000000
#define HOURS_TO_SECONDS 3600
#define SLEEP_TIME 6 * HOURS_TO_SECONDS* SECONDS_TO_MICROSECONDS // 6 hours

void app_main(void)
{
	// Initialize NVS
	esp_err_t esp_err = nvs_flash_init();
	if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES || esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		esp_err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(esp_err);

	// config sleep timer wake up
	esp_err = esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME);
	if (esp_err != ESP_OK) {
		ESP_LOGE(LOG_TAG_MAIN, "Error enabling timer wakeup for deep sleep.");
		return;
	}

	epd_init(EPD_OPTIONS_DEFAULT);

	// measure battery voltage
	epd_poweron();
	vTaskDelay(pdMS_TO_TICKS(10 * 1000)); // wait for power to stabilize

	adc_oneshot_unit_handle_t adc_handle;
	adc_oneshot_unit_init_cfg_t adc_config = {
		.unit_id = ADC_UNIT_2,
		.ulp_mode = ADC_ULP_MODE_DISABLE,
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_handle));

	adc_oneshot_chan_cfg_t adc_channel_config = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_12,
	};
	// channel 3 for ADC2 is GPIO14 which is connected to battery voltage divider
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &adc_channel_config));

	int raw = 0;
	ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &raw));
	float battery_voltage = ((float)raw / 4095.0f) * 3.3f * 2.0f * (1100.0f / 1000.0f); // vref 1.1V

	float battery_percentage;
	if (battery_voltage >= 4.2f) {
		battery_percentage = 100.0f;
	} else if (battery_voltage <= 3.0f) {
		battery_percentage = 0.0f;
	} else {
		battery_percentage = ((battery_voltage - 3.0f) / (4.2f - 3.0f)) *
							 100.0f; // assume battery voltage range 3.0V - 4.2V, needs validation
	}
	ESP_LOGW(LOG_TAG_MAIN,
			 "Battery voltage: %.2f V, Battery percentage: %.2f %% ",
			 battery_voltage,
			 battery_percentage);
	ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
	epd_poweroff();

	// Connect to WiFi
	uint8_t err = connect_wifi();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error connecting to WiFi.");
		return;
	}

	// button_switch_context_init();

	// setup UI
	err = init_ui(battery_percentage);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error initializing UI.");
		return;
	}

	// start refresh task
	err = start_refresh_task();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error starting refresh task.");
		return;
	}

	// start location task
	err = start_location_task();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error starting location task.");
		return;
	}

	// start weather task
	err = start_weather_tasks();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error starting weather task.");
		return;
	}
}