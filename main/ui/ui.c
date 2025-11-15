// ESP includes
#include "esp_log.h"

// EPD driver includes
#include "epd_highlevel.h"

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
static EpdiyHighlevelState hl;
static uint8_t* fb;

static SemaphoreHandle_t fb_mutex;
static EventGroupHandle_t ui_cycle_group;

static const EpdFont* const font_11 = &SegoeVF_11;
static const EpdFont* const font_9 = &SegoeVF_9;

static EpdFontProperties header_font_props;
static EpdFontProperties subtitle_font_props;

uint8_t init_ui()
{
	// setup
	epd_init(EPD_OPTIONS_DEFAULT);
	hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);

	fb = epd_hl_get_framebuffer(&hl);

	fb_mutex = xSemaphoreCreateMutex();
	if (fb_mutex == NULL) {
		ESP_LOGE(LOG_TAG_UI, "Error creating framebuffer mutex.");
		return 1;
	}

	ui_cycle_group = xEventGroupCreate();
	if (ui_cycle_group == NULL) {
		ESP_LOGE(LOG_TAG_UI, "Error creating UI cycle event group.");
		return 1;
	}

	// define font properties
	header_font_props = epd_font_properties_default();
	subtitle_font_props = epd_font_properties_default();
	subtitle_font_props.fg_color = 5; // mid gray

	epd_poweron();

	// clear screen
	epd_fullclear(&hl, TEMPERATURE);

	// place on screen base elements
	uint8_t err = populate_base_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_UI, "Error populating base UI.");
		epd_poweroff();
		return 1;
	}

	enum EpdDrawError epd_err = epd_hl_update_screen(&hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error updating screen. EPD error code: %d", epd_err);
		epd_poweroff();
		return 1;
	}

	epd_poweroff();
	return 0;
}

uint8_t populate_base_ui()
{
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

	// draw calendar icon
	EpdRect calendar_icon = {
		.x = 15, .y = 13, .width = calendar_width, .height = calendar_height
	};

	epd_copy_to_framebuffer(calendar_icon, calendar_data, fb);

	// draw center line
	epd_draw_vline(EPD_WIDTH / 2, 0, EPD_HEIGHT, MID_GRAY, fb);

	// populate weather tab
	uint8_t err = populate_weather_tab_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_UI, "Error populating weather tab UI.");
		return 1;
	}

	return 0;
}

uint8_t populate_weather_tab_ui()
{
	// draw weather icon
	EpdRect weather_icon = {
		.x = 15 + EPD_WIDTH / 2, .y = 14, .width = sun_width, .height = sun_height
	};

	epd_copy_to_framebuffer(weather_icon, sun_data, fb);

	// write weather
	int cursor_x = 50 + EPD_WIDTH / 2;
	int cursor_y = 32;
	char weather[] = "Weather";

	enum EpdDrawError epd_err =
	  epd_write_string(font_11, weather, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting weather string. EPD error code: %d", epd_err);
		return 1;
	}

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

uint8_t write_location_ui(const char* city, const char* country_code)
{
	// write city
	int cursor_x = 52 + EPD_WIDTH / 2;
	int cursor_y = 62;
	char location[64];
	sprintf(location, "%s, %s", city, country_code);

	ESP_LOGD(LOG_TAG_UI, "%s", location);

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	enum EpdDrawError epd_err =
	  epd_write_string(font_9, location, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting location string. EPD error code: %d", epd_err);
		return 1;
	}

	// signal location done
	xEventGroupSetBits(ui_cycle_group, LOCATION_DONE_BIT);

	return 0;
}

uint8_t refresh_weather_tab_ui()
{
	xEventGroupWaitBits(ui_cycle_group,
						LOCATION_DONE_BIT,
						pdTRUE, // clear bits
						pdTRUE, // wait for all bits
						portMAX_DELAY);
	epd_poweron();

	// Estimate of area to update
	EpdRect location_area = { .x = 52 + EPD_WIDTH / 2, .y = 42, .width = 200, .height = 30 };

	enum EpdDrawError epd_err =
	  epd_hl_update_area(&hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE, location_area);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error updating screen. EPD error code: %d", epd_err);
		epd_poweroff();
		return 1;
	}

	epd_poweroff();
	return 0;
}

uint8_t write_current_weather_ui()
{
	return 0;
}