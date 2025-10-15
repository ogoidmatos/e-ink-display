// Standard includes
#include <stdio.h>

// ESP includes
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

// Own includes
#include "ui/ui.h"
#include "utils/button.h"
#include "utils/network_manager.h"

#define LOG_TAG_MAIN "MAIN"

EpdiyHighlevelState hl;

void app_main(void)
{
	// Initialize NVS
	esp_err_t esp_err = nvs_flash_init();
	if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES || esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		esp_err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(esp_err);

	// Connect to WiFi
	int err = connect_wifi();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error connecting to WiFi.");
		return;
	}

	button_switch_context_init();

	// setup UI
	err = ui_init(&hl);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error initializing UI.");
		return;
	}
}