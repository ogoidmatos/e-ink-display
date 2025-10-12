#include <stdio.h>

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

#include "ui.h"

static const EpdFont* font_11 = &SegoeVF_11;
static const EpdFont* font_9 = &SegoeVF_9;

void ui_init(EpdiyHighlevelState* hl)
{
	// setup
	epd_init(EPD_OPTIONS_DEFAULT);
	*hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);

	fb = epd_hl_get_framebuffer(hl);

	epd_poweron();

	// clear screen
	epd_fullclear(hl, TEMPERATURE);

	// place on screen base elements
	populate_base_ui();

	enum EpdDrawError err = epd_hl_update_screen(hl, MODE_EPDIY_WHITE_TO_GL16, TEMPERATURE);

	if (err != EPD_DRAW_SUCCESS) {
		printf("Error updating screen: %d\n", err);
	}

	epd_poweroff();
}

void populate_base_ui()
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

	epd_write_string(font_11, meetings, &cursor_x, &cursor_y, fb, &header_font_props);

	// write date
	// TODO: make dynamic
	cursor_x = 52;
	cursor_y = 62;
	char date[] = "Monday, 1 Jan 2024";

	epd_write_string(font_9, date, &cursor_x, &cursor_y, fb, &subtitle_font_props);

	// write weather
	cursor_x = 50 + EPD_WIDTH / 2;
	cursor_y = 32;
	char weather[] = "Weather";

	epd_write_string(font_11, weather, &cursor_x, &cursor_y, fb, &header_font_props);

	// write city
	// TODO: make dynamic based on config
	cursor_x = 52 + EPD_WIDTH / 2;
	cursor_y = 62;
	char city[] = "Lisbon, PT";

	epd_write_string(font_9, city, &cursor_x, &cursor_y, fb, &subtitle_font_props);

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

	epd_write_string(font_9, upcoming, &cursor_x, &cursor_y, fb, &header_font_props);

	// draw forecast widgets
	EpdRect forecast_base_widget = { .x = today_base_widget.x,
									 .y = cursor_y,
									 .width = forecast_base_width,
									 .height = forecast_base_height };

	epd_copy_to_framebuffer(forecast_base_widget, forecast_base_data, fb);

	forecast_base_widget.y += forecast_base_height + 20;

	epd_copy_to_framebuffer(forecast_base_widget, forecast_base_data, fb);
}