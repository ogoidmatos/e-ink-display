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
#include "fonts/segoevf_24.h"
#include "fonts/segoevf_9.h"

// Icon includes
#include "icons/arrow_down.h"
#include "icons/arrow_up.h"
#include "icons/calendar.h"
#include "icons/weather_icons.h"
#include "icons/weather_icons_large.h"

// Own includes
#include "ui.h"

// Static variables
static EpdiyHighlevelState hl;
static uint8_t* fb;

static SemaphoreHandle_t fb_mutex;

static const EpdFont* const font_24 = &SegoeVF_24;
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

	// define font properties
	header_font_props = epd_font_properties_default();
	subtitle_font_props = epd_font_properties_default();
	subtitle_font_props.fg_color = 5; // mid gray

	epd_poweron();
	// clear screen
	epd_fullclear(&hl, TEMPERATURE);
	epd_poweroff();

	// place on screen base elements
	uint8_t err = populate_base_ui();
	if (err != 0) {
		ESP_LOGE(LOG_TAG_UI, "Error populating base UI.");
		return 1;
	}

	return 0;
}

void draw_fancy_rect(EpdRect rect, uint8_t margin, uint8_t color, uint8_t* framebuffer)
{
	epd_draw_hline(rect.x + margin, rect.y, rect.width - 2 * margin, color, framebuffer);
	epd_draw_hline(
	  rect.x + margin, rect.y + rect.height - 1, rect.width - 2 * margin, color, framebuffer);
	epd_draw_vline(rect.x, rect.y + margin, rect.height - 2 * margin, color, framebuffer);
	epd_draw_vline(
	  rect.x + rect.width - 1, rect.y + margin, rect.height - 2 * margin, color, framebuffer);
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

	enum EpdDrawError epd_err =
	  epd_write_string(font_11, "Weather", &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting weather string. EPD error code: %d", epd_err);
		return 1;
	}

	// draw today weather widget
	EpdRect today_base_widget = { .x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2,
								  .y = 0.15 * EPD_HEIGHT,
								  .width = CURRENT_WEATHER_WIDGET_WIDTH,
								  .height = CURRENT_WEATHER_WIDGET_HEIGHT };

	// epd_copy_to_framebuffer(today_base_widget, today_base_data, fb);
	draw_fancy_rect(today_base_widget, 10, BLACK, fb);

	// write upcoming
	cursor_x = today_base_widget.x;
	cursor_y = today_base_widget.y + CURRENT_WEATHER_WIDGET_HEIGHT + 35;

	epd_err = epd_write_string(font_9, "Upcoming", &cursor_x, &cursor_y, fb, &header_font_props);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting upcoming string. EPD error code: %d", epd_err);
		return 1;
	}

	// draw forecast widgets
	EpdRect forecast_base_widget = { .x = today_base_widget.x,
									 .y = cursor_y,
									 .width = FORECAST_WEATHER_WIDGET_WIDTH,
									 .height = FORECAST_WEATHER_WIDGET_HEIGHT };

	draw_fancy_rect(forecast_base_widget, 10, BLACK, fb);

	forecast_base_widget.y += FORECAST_WEATHER_WIDGET_HEIGHT + 20;

	draw_fancy_rect(forecast_base_widget, 10, BLACK, fb);

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

	return 0;
}

uint8_t day_of_the_week(uint8_t d, uint8_t m, uint16_t y)
{
	y -= m < 3;
	return (y + y / 4 - y / 100 + y / 400 + "-bed=pen+mad."[m] + d) % 7;
}

uint8_t write_date_ui(uint16_t year, uint8_t month, uint8_t day)
{
	// write date
	int cursor_x = 52;
	int cursor_y = 62;
	char date[32];
	const char month_str[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
									"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	const char day_str[7][10] = { "Sunday",	  "Monday", "Tuesday", "Wednesday",
								  "Thursday", "Friday", "Saturday" };

	sprintf(date,
			"%s, %02d %s %04d",
			day_str[day_of_the_week(day, month, year)],
			day,
			month_str[month - 1],
			year);

	ESP_LOGD(LOG_TAG_UI, "%s", date);

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	enum EpdDrawError epd_err =
	  epd_write_string(font_9, date, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting date string. EPD error code: %d", epd_err);
		return 1;
	}

	return 0;
}

uint8_t refresh_screen_ui()
{
	epd_poweron();

	enum EpdDrawError epd_err = epd_hl_update_screen(&hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error updating screen. EPD error code: %d", epd_err);
		epd_poweroff();
		return 1;
	}

	epd_poweroff();
	return 0;
}

const uint8_t* process_weather_icon(const char* weather_code, const int is_day_time, bool is_large)
{
	if (strcmp(weather_code, "CLEAR") == 0) {
		if (is_day_time) {
			// sun icon
			return is_large ? sun_large_data : sun_data;
		} else {
			// moon icon
			return is_large ? moon_large_data : moon_data;
		}
	} else if (strcmp(weather_code, "PARTLY_CLOUDY") == 0 ||
			   strcmp(weather_code, "MOSTLY_CLEAR") == 0) {
		if (is_day_time) {
			// cloud sun icon
			return is_large ? cloud_sun_large_data : cloud_sun_data;
		} else {
			// cloud moon icon
			return is_large ? cloud_moon_large_data : cloud_moon_data;
		}
	} else if (strcmp(weather_code, "WINDY") == 0) {
		// wind icon
		return is_large ? wind_large_data : wind_data;
	} else if (strcmp(weather_code, "MOSTLY_CLOUDY") == 0 || strcmp(weather_code, "CLOUDY") == 0) {
		// cloudy icon
		return is_large ? cloudy_large_data : cloudy_data;
	} else if (strcmp(weather_code, "LIGHT_RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "CHANCE_OF_SHOWERS") == 0 ||
			   strcmp(weather_code, "SCATTERED_SHOWERS") == 0 ||
			   strcmp(weather_code, "LIGHT_RAIN") == 0) {
		// light rain icon
		return is_large ? cloud_drizzle_large_data : cloud_drizzle_data;
	} else if (strcmp(weather_code, "WIND_AND_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "HEAVY_RAIN_SHOWERS") == 0 ||
			   strcmp(weather_code, "LIGHT_TO_MODERATE_RAIN") == 0 ||
			   strcmp(weather_code, "MODERATE_TO_HEAVY_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN") == 0 || strcmp(weather_code, "HEAVY_RAIN") == 0 ||
			   strcmp(weather_code, "RAIN_PERIODICALLY_HEAVY") == 0) {
		// heavy rain icon
		return is_large ? cloud_rain_large_data : cloud_rain_data;
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
		return is_large ? snowflake_large_data : snowflake_data;
	} else if (strcmp(weather_code, "HAIL") == 0 || strcmp(weather_code, "HAIL_SHOWERS") == 0) {
		// hail icon
		return is_large ? cloud_hail_large_data : cloud_hail_data;
	} else if (strcmp(weather_code, "THUNDERSTORM") == 0 ||
			   strcmp(weather_code, "THUNDERSHOWER") == 0 ||
			   strcmp(weather_code, "LIGHT_THUNDERSTORM_RAIN") == 0 ||
			   strcmp(weather_code, "SCATTERED_THUNDERSTORMS") == 0 ||
			   strcmp(weather_code, "HEAVY_THUNDERSTORM") == 0) {
		// thunderstorm icon
		return is_large ? cloud_lightning_large_data : cloud_lightning_data;
	} else {
		// default icon
		return is_large ? sun_large_data : sun_data;
	}
}

uint8_t write_current_weather_ui(const current_weather_t* weather)
{
	const int box_x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2;
	const int box_y = 0.15 * EPD_HEIGHT;
	// draw horizontal divider
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_draw_hline(box_x + (int)(0.05 * CURRENT_WEATHER_WIDGET_WIDTH),
				   box_y + 0.8 * CURRENT_WEATHER_WIDGET_HEIGHT,
				   0.9 * CURRENT_WEATHER_WIDGET_WIDTH,
				   MID_GRAY,
				   fb);
	xSemaphoreGive(fb_mutex);

	// draw additional information icons
	int icon_y_low = 0.85 * CURRENT_WEATHER_WIDGET_HEIGHT + box_y;
	int icon_y_high = 0.65 * CURRENT_WEATHER_WIDGET_HEIGHT + box_y;

	int icon_x = box_x + 0.05 * CURRENT_WEATHER_WIDGET_WIDTH;

	EpdRect weather_icon = {
		.x = icon_x, .y = icon_y_low, .width = weather_icon_width, .height = weather_icon_height
	};

	char buffer[32];
	int cursor_x = icon_x + weather_icon_width + 5;
	int cursor_y = icon_y_low + weather_icon_height - 5;

	// humidity
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, droplets_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "%3d %%", weather->humidity);

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	enum EpdDrawError epd_err =
	  epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting humidity. EPD error code: %d", epd_err);
		return 1;
	}
	int next_icon_x = cursor_x + 20;

	// maximum temperature
	weather_icon.y = icon_y_high;
	cursor_x = icon_x + weather_icon_width + 5;
	cursor_y = icon_y_high + weather_icon_height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, arrow_up_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "%2.1f ºC", weather->max_temperature_c);

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting maximum temperature. EPD error code: %d", epd_err);
		return 1;
	}

	// wind speed
	weather_icon.x = next_icon_x;
	weather_icon.y = icon_y_low;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, wind_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "%2d kph", weather->wind_speed_kph);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting wind speed. EPD error code: %d", epd_err);
		return 1;
	}
	next_icon_x = cursor_x + 20;

	// minimum temperature
	weather_icon.y = icon_y_high;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, arrow_down_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "%2.1f ºC", weather->min_temperature_c);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting minimum temperature. EPD error code: %d", epd_err);
		return 1;
	}

	// rain chance
	weather_icon.x = next_icon_x;
	weather_icon.y = icon_y_low;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, cloud_rain_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "%2d %%", weather->rain_chance);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting rain chance. EPD error code: %d", epd_err);
		return 1;
	}
	next_icon_x = cursor_x + 20;

	// sunrise - TODO: FINALIZE WITH DYNAMIC DATA
	weather_icon.y = icon_y_high;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, sunrise_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "10:10");
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting sunrise time. EPD error code: %d", epd_err);
		return 1;
	}

	// UV index
	weather_icon.x = next_icon_x;
	weather_icon.y = icon_y_low;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, sun_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "UV %d", weather->uv_index);
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting UV index. EPD error code: %d", epd_err);
		return 1;
	}

	// sunset - TODO: FINALIZE WITH DYNAMIC DATA
	weather_icon.y = icon_y_high;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, sunset_data, fb);
	xSemaphoreGive(fb_mutex);
	sprintf(buffer, "19:00");
	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting sunset time. EPD error code: %d", epd_err);
		return 1;
	}

	// draw main weather icon
	weather_icon = (EpdRect){ .x = box_x + 0.7 * CURRENT_WEATHER_WIDGET_WIDTH,
							  .y = box_y + 0.1 * CURRENT_WEATHER_WIDGET_HEIGHT,
							  .width = weather_large_width,
							  .height = weather_large_height };
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(
	  weather_icon, process_weather_icon(weather->weather_code, weather->is_day_time, true), fb);
	xSemaphoreGive(fb_mutex);

	// write temperature
	sprintf(buffer, "%2.1fº", weather->temperature_c);
	cursor_x = box_x + 0.05 * CURRENT_WEATHER_WIDGET_WIDTH;
	cursor_y = box_y + 0.3 * CURRENT_WEATHER_WIDGET_HEIGHT;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_24, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting temperature. EPD error code: %d", epd_err);
		return 1;
	}

	// write weather description
	cursor_x = box_x + 0.05 * CURRENT_WEATHER_WIDGET_WIDTH;
	cursor_y = box_y + 0.45 * CURRENT_WEATHER_WIDGET_HEIGHT;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err =
	  epd_write_string(font_9, weather->description, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting weather description. EPD error code: %d", epd_err);
		return 1;
	}

	// write feels like temperature
	sprintf(buffer, "Feels like %2.1fº", weather->feels_like_temperature_c);
	cursor_x = box_x + 0.05 * CURRENT_WEATHER_WIDGET_WIDTH;
	cursor_y = box_y + 0.57 * CURRENT_WEATHER_WIDGET_HEIGHT;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting feels like temperature. EPD error code: %d", epd_err);
		return 1;
	}

	return 0;
}

uint8_t write_forecast_ui(const forecast_weather_t* forecast_array)
{
	int forecast_x = 15 + weather_icon_width / 2 + EPD_WIDTH / 2; // same as today weather box x
	int first_forecast_y = 361;
	int second_forecast_y = 441;

	// draw first forecast
	EpdRect weather_icon = { .x = forecast_x + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH,
							 .y = first_forecast_y + 0.3 * FORECAST_WEATHER_WIDGET_HEIGHT,
							 .width = weather_icon_width,
							 .height = weather_icon_height };

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(
	  weather_icon, process_weather_icon(forecast_array[1].weather_code, 1, false), fb);
	xSemaphoreGive(fb_mutex);

	int cursor_x = weather_icon.x + weather_icon.width + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH;
	int cursor_y = weather_icon.y + 8;

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	enum EpdDrawError epd_err =
	  epd_write_string(font_9, "Tomorrow", &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting forecast 1 date. EPD error code: %d", epd_err);
		return 1;
	}
	cursor_x = weather_icon.x + weather_icon.width + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH;
	cursor_y = weather_icon.y + weather_icon.height + 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(
	  font_9, forecast_array[1].description, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(
		  LOG_TAG_UI, "Error writting forecast 1 weather description. EPD error code: %d", epd_err);
		return 1;
	}

	uint8_t dimmed_rain_icon[weather_icon_width * weather_icon_height / 2];
	for (int i = 0; i < weather_icon_width * weather_icon_height / 2; i++) {
		uint8_t first_pixel = (cloud_rain_data[i] & 0xF0) >> 4;
		uint8_t second_pixel = (cloud_rain_data[i] & 0x0F);
		first_pixel = first_pixel * 3;
		first_pixel = first_pixel > 0xF ? 0xF : first_pixel;
		second_pixel = second_pixel * 3;
		second_pixel = second_pixel > 0xF ? 0xF : second_pixel;
		dimmed_rain_icon[i] = ((first_pixel << 4) & 0xF0) | ((second_pixel) & 0x0F);
	}

	// draw rain change for first forecast
	weather_icon.x = forecast_x + 0.6 * FORECAST_WEATHER_WIDGET_WIDTH;
	weather_icon.y = first_forecast_y + 0.3 * FORECAST_WEATHER_WIDGET_HEIGHT;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, dimmed_rain_icon, fb);
	xSemaphoreGive(fb_mutex);

	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;

	char buffer[16];
	sprintf(buffer, "%2d %%", forecast_array[1].rain_chance);
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting forecast 1 rain chance. EPD error code: %d", epd_err);
		return 1;
	}

	// draw max/min temperature for first forecast
	sprintf(buffer,
			"%2.0fº / %2.0fº",
			forecast_array[1].max_temperature_c,
			forecast_array[1].min_temperature_c);

	cursor_x += 15;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(
		  LOG_TAG_UI, "Error writting forecast 1 max/min temperature. EPD error code: %d", epd_err);
		return 1;
	}

	// draw second forecast
	weather_icon.x = forecast_x + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH;
	weather_icon.y = second_forecast_y + 0.3 * FORECAST_WEATHER_WIDGET_HEIGHT;

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(
	  weather_icon, process_weather_icon(forecast_array[2].weather_code, 1, false), fb);
	xSemaphoreGive(fb_mutex);

	cursor_x = weather_icon.x + weather_icon.width + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH;
	cursor_y = weather_icon.y + 8;

	const char day_str[7][10] = { "Sunday",	  "Monday", "Tuesday", "Wednesday",
								  "Thursday", "Friday", "Saturday" };

	sprintf(
	  buffer,
	  "%s",
	  day_str[day_of_the_week(
		forecast_array[2].date.day, forecast_array[2].date.month, forecast_array[2].date.year)]);

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting forecast 2 date. EPD error code: %d", epd_err);
		return 1;
	}

	cursor_x = weather_icon.x + weather_icon.width + 0.05 * FORECAST_WEATHER_WIDGET_WIDTH;
	cursor_y = weather_icon.y + weather_icon.height + 5;

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(
	  font_9, forecast_array[2].description, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(
		  LOG_TAG_UI, "Error writting forecast 2 weather description. EPD error code: %d", epd_err);
		return 1;
	}

	// draw rain change for second forecast
	weather_icon.x = forecast_x + 0.6 * FORECAST_WEATHER_WIDGET_WIDTH;
	weather_icon.y = second_forecast_y + 0.3 * FORECAST_WEATHER_WIDGET_HEIGHT;

	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_copy_to_framebuffer(weather_icon, dimmed_rain_icon, fb);
	xSemaphoreGive(fb_mutex);

	cursor_x = weather_icon.x + weather_icon.width + 5;
	cursor_y = weather_icon.y + weather_icon.height - 5;

	sprintf(buffer, "%2d %%", forecast_array[2].rain_chance);
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &subtitle_font_props);
	xSemaphoreGive(fb_mutex);

	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(LOG_TAG_UI, "Error writting forecast 2 rain chance. EPD error code: %d", epd_err);
		return 1;
	}

	// draw max/min temperature for second forecast
	sprintf(buffer,
			"%2.0fº / %2.0fº",
			forecast_array[2].max_temperature_c,
			forecast_array[2].min_temperature_c);
	cursor_x += 15;
	cursor_y = weather_icon.y + weather_icon.height - 5;
	xSemaphoreTake(fb_mutex, portMAX_DELAY);
	epd_err = epd_write_string(font_9, buffer, &cursor_x, &cursor_y, fb, &header_font_props);
	xSemaphoreGive(fb_mutex);
	if (epd_err != EPD_DRAW_SUCCESS) {
		ESP_LOGE(
		  LOG_TAG_UI, "Error writting forecast 2 max/min temperature. EPD error code: %d", epd_err);
		return 1;
	}

	return 0;
}