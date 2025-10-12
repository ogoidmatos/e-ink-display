// ESP includes
#include "esp_log.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Font includes
#include "fonts/segoevf_11.h"
#include "fonts/segoevf_9.h"

// Icon includes
#include "icons/calendar.h"
#include "icons/sun.h"

// Widget includes
#include "widgets/forecast_base.h"
#include "widgets/today_base.h"

// Own includes
#include "ui.h"

// Static variables
static uint8_t* fb;
static const EpdFont* const font_11 = &SegoeVF_11;
static const EpdFont* const font_9 = &SegoeVF_9;

int ui_init(EpdiyHighlevelState* hl)
{
	// setup
	epd_init(EPD_OPTIONS_DEFAULT);
	*hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);

	fb = epd_hl_get_framebuffer(hl);

	epd_poweron();

	// clear screen
	epd_fullclear(hl, TEMPERATURE);

	// place on screen base elements
	int err = populate_base_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_UI, "Error populating base UI.");
		epd_poweroff();
		return 1;
	}

	enum EpdDrawError epd_err = epd_hl_update_screen(hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error updating screen. EPD error code: %d", epd_err);
		epd_poweroff();
		return 1;
	}

	epd_poweroff();
	return 0;
}

int populate_base_ui()
{
	// populate base UI
	// define font properties
	EpdFontProperties header_font_props = epd_font_properties_default();
	EpdFontProperties subtitle_font_props = epd_font_properties_default();
	subtitle_font_props.fg_color = 5; // mid gray

	// write Today's meeting string
	int cursor_x = 50;
	int cursor_y = 32;
	char meetings[] = "Today's Meetings";

	enum EpdDrawError epd_err =
	  epd_write_string(font_11, meetings, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting meetings string. EPD error code: %d", epd_err);
		return 1;
	}

	// write date
	// TODO: make dynamic
	cursor_x = 52;
	cursor_y = 62;
	char date[] = "Monday, 1 Jan 2024";

	epd_err = epd_write_string(font_9, date, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting date string. EPD error code: %d", epd_err);
		return 1;
	}

	// write weather
	cursor_x = 50 + EPD_WIDTH / 2;
	cursor_y = 32;
	char weather[] = "Weather";

	epd_err = epd_write_string(font_11, weather, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting weather string. EPD error code: %d", epd_err);
		return 1;
	}

	// write city
	// TODO: make dynamic based on config
	cursor_x = 52 + EPD_WIDTH / 2;
	cursor_y = 62;
	char city[] = "Lisbon, PT";

	epd_err = epd_write_string(font_9, city, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting city string. EPD error code: %d", epd_err);
		return 1;
	}

	// draw center line
	epd_draw_vline(EPD_WIDTH / 2, 0, EPD_HEIGHT, MID_GRAY, fb);

	// draw calendar icon
	EpdRect calendar_icon = {
		.x = 15, .y = 13, .width = calendar_width, .height = calendar_height
	};

	epd_copy_to_framebuffer(calendar_icon, calendar_data, fb);

	// draw weather icon
	EpdRect weather_icon = {
		.x = 15 + EPD_WIDTH / 2, .y = 14, .width = sun_width, .height = sun_height
	};

	epd_copy_to_framebuffer(weather_icon, sun_data, fb);

	// draw today weather widget
	EpdRect today_base_widget = { .x = 15 + sun_width / 2 + EPD_WIDTH / 2,
								  .y = 85,
								  .width = today_base_width,
								  .height = today_base_height };

	epd_copy_to_framebuffer(today_base_widget, today_base_data, fb);

	// write upcoming
	cursor_x = today_base_widget.x;
	cursor_y = today_base_widget.y + today_base_height + 35;
	char upcoming[] = "Upcoming";

	epd_err = epd_write_string(font_9, upcoming, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting upcoming string. EPD error code: %d", epd_err);
		return 1;
	}

	// draw forecast widgets
	EpdRect forecast_base_widget = { .x = today_base_widget.x,
									 .y = cursor_y,
									 .width = forecast_base_width,
									 .height = forecast_base_height };

	epd_copy_to_framebuffer(forecast_base_widget, forecast_base_data, fb);

	forecast_base_widget.y += forecast_base_height + 20;

	epd_copy_to_framebuffer(forecast_base_widget, forecast_base_data, fb);

	return 0;
}