#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "base.h"

#include "ui/ui.h"

EpdiyHighlevelState hl;

void app_main(void)
{
	// setup UI
	ui_init(&hl);

	// EpdRect base_img = {
	//     .x = 0,
	//     .y = 0,
	//     .width = base_width,
	//     .height = base_height
	// };

	// epd_fullclear(&hl, temperature);

	// epd_copy_to_framebuffer(base_img, base_data, fb);

	// vTaskDelay(pdMS_TO_TICKS(1000));
	// epd_hl_set_all_white(&hl);
	// epd_clear();
	// vTaskDelay(pdMS_TO_TICKS(1000));

	// epd_fill_rect(rect, BLACK, fb);
}