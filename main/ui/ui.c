// ESP includes
#include "epd_driver.h"
#include "epd_internals.h"
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
#include "icons/weather_icons.h"

// Widget includes
#include "widgets/forecast_base.h"
#include "widgets/today_base.h"
#include <stdio.h>
#include <string.h>

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
		.x = 15 + EPD_WIDTH / 2, .y = 14, .width = weather_icon_width, .height = weather_icon_height
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
	EpdRect today_base_widget = { .x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2,
								  .y = 0.15 * EPD_HEIGHT,
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
						LOCATION_DONE_BIT | CURRENT_WEATHER_DONE_BIT,
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

	location_area = (EpdRect){ .x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2,
							   .y = 0.15 * EPD_HEIGHT,
							   .width = today_base_width,
							   .height = today_base_height };

	epd_err = epd_hl_update_area(&hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE, location_area);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error updating screen. EPD error code: %d", epd_err);
		epd_poweroff();
		return 1;
	}

	epd_poweroff();
	return 0;
}

const uint8_t* process_weather_icon(const char* weather_code, const int is_day_time)
{
	if (strcmp(weather_code, "CLEAR") == 0 || strcmp(weather_code, "MOSTLY_CLEAR") == 0) {
		if (is_day_time) {
			// sun icon
			return sun_data;
		} else {
			// moon icon
			return moon_data;
		}
	} else if (strcmp(weather_code, "PARTLY_CLOUDY") == 0 ||
			   strcmp(weather_code, "MOSTLY_CLOUDY") == 0 || strcmp(weather_code, "CLOUDY") == 0) {
		// cloudy icon
		return cloudy_data;
	} else if (strcmp(weather_code, "WINDY") == 0) {
		// wind icon
		return wind_data;
	} else if (strcmp(weather_code, "CLOUDY") == 0) {
		if (is_day_time) {
			// cloud sun icon
			return cloudy_data;
		} else {
			// cloud moon icon
			return cloud_moon_data;
		}
	} else if (strcmp(weather_code, "LIGHT_RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "CHANCE_OF_SHOWERS") == 0 ||
			   strcmp(weather_code, "SCATTERED_SHOWERS") == 0 ||
			   strcmp(weather_code, "LIGHT_RAIN") == 0) {
		// light rain icon
		return cloud_drizzle_data;
	} else if (strcmp(weather_code, "WIND_AND_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "HEAVY_RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "LIGHT_TO_MODERATE_RAIN") == 0 ||
			   strcmp(weather_code, "MODERATE_TO_HEAVY_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN") == 0 || strcmp(weather_code, "HEAVY_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN_PERIODICALLY_HEAVY") == 0) {
		// heavy rain icon
		return cloud_rain_data;
	} else if (strcmp(weather_code, "LIGHT_SNOW_SHOWERS") == 0 ||
			   strcmp(weather_code, "CHANCE_OF_SNOW_SHOWERS") == 0 ||
			   strcmp(weather_code, "SCATTERED_SNOW_SHOWERS") == 0 ||
			   strcmp(weather_code, "SNOW_SHOWERS") == 0 ||
			   strcmp(weather_code, "HEAVY_SNOW_SHOWERS") == 0 ||
			   strcmp(weather_code, "LIGHT_TO_MODERATE_SNOW") == 0 ||
			   strcmp(weather_code, "MODERATE_TO_HEAVY_SNOW") == 0 ||
			   strcmp(weather_code, "SNOW") == 0 || strcmp(weather_code, "LIGHT_SNOW") == 0 ||
			   strcmp(weather_code, "HEAVY_SNOW") == 0 || strcmp(weather_code, "SNOWSTORM") == 0 ||
			   strcmp(weather_code, "SNOW_PERIODICALLY_HEAVY") == 0 ||
			   strcmp(weather_code, "HEAVY_SNOW_STORM") == 0 ||
			   strcmp(weather_code, "BLOWING_SNOW") == 0 ||
			   strcmp(weather_code, "RAIN_AND_SNOW") == 0) {
		// snow icon
		return snowflake_data;
	} else if (strcmp(weather_code, "HAIL") == 0 || strcmp(weather_code, "HAIL_SHOWERS") == 0) {
		// hail icon
		return cloud_hail_data;
	} else if (strcmp(weather_code, "THUNDERSTORM") == 0 ||
			   strcmp(weather_code, "THUNDERSHOWER") == 0 ||
			   strcmp(weather_code, "LIGHT_THUNDERSTORM_RAIN") == 0 ||
			   strcmp(weather_code, "SCATTERED_THUNDERSTORMS") == 0 ||
			   strcmp(weather_code, "HEAVY_THUNDERSTORM") == 0) {
		// thunderstorm icon
		return cloud_lightning_data;
	} else {
		// default icon
		return sun_data;
	}
}

uint8_t write_current_weather_ui(const current_weather_t* weather)
{
	const int box_x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2;
	const int box_y = 0.15 * EPD_HEIGHT;
	// draw horizontal divider
	epd_draw_hline(box_x + (int)(0.05 * today_base_width),
				   box_y + 0.75 * today_base_height,
				   0.9 * today_base_width,
				   MID_GRAY,
				   fb);

	// draw additional information icons
	const int icon_y = 0.8 * today_base_height + box_y;
	int icon_x = box_x + 0.05 * today_base_width;
	int icon_spacing = 0.9 * today_base_width / 4;

	EpdRect weather_icon = {
		.x = icon_x, .y = icon_y, .width = weather_icon_width, .height = weather_icon_height
	};

	char buffer[16];
	int cursor_x = icon_x + weather_icon_width + 5;
	int cursor_y = icon_y + weather_icon_height - 5;

	// humidity
	epd_copy_to_framebuffer(weather_icon, droplets_data, fb);
	sprintf(buffer, "%3d %%", weather->humidity);

	enum EpdDrawError epd_err =
	  epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting location string. EPD error code: %d", epd_err);
		return 1;
	}

	// wind speed
	weather_icon.x = cursor_x + 20;
	epd_copy_to_framebuffer(weather_icon, wind_data, fb);
	sprintf(buffer, "%2d kph", weather->wind_speed_kph);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting location string. EPD error code: %d", epd_err);
		return 1;
	}

	// rain chance
	weather_icon.x = cursor_x + 20;
	epd_copy_to_framebuffer(weather_icon, cloud_rain_data, fb);
	sprintf(buffer, "%2d %%", weather->rain_chance);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting location string. EPD error code: %d", epd_err);
		return 1;
	}

	// UV index
	weather_icon.x = cursor_x + 20;
	epd_copy_to_framebuffer(weather_icon, sun_data, fb);
	sprintf(buffer, "UV %d", weather->uv_index);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting location string. EPD error code: %d", epd_err);
		return 1;
	}

	// signal current weather done
	xEventGroupSetBits(ui_cycle_group, CURRENT_WEATHER_DONE_BIT);
	return 0;
}