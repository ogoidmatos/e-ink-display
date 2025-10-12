// Standard includes
#include <stdio.h>

// ESP includes
#include "esp_log.h"

// Own includes
#include "ui/ui.h"
#include "utils/button.h"
#include "utils/network_manager.h"

#define LOG_TAG_MAIN "MAIN"

EpdiyHighlevelState hl;

void app_main(void)
{
	button_switch_context_init();

	// setup UI
	int err = ui_init(&hl);
	if (err != 0) {
		ESP_LOGE(LOG_TAG_MAIN, "Error initializing UI.");
		return;
	}
}