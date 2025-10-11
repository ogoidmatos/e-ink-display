#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "../fonts/segoevf_11.h"
#include "../fonts/segoevf_9.h"

static const EpdFont* font_11 = &SegoeVF_11;
static const EpdFont* font_9 = &SegoeVF_9;

void ui_init(EpdiyHighlevelState* hl){
    // setup
    epd_init(EPD_OPTIONS_DEFAULT);
    *hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);

    fb = epd_hl_get_framebuffer(hl);

    epd_poweron();

    // clear screen
    epd_fullclear(hl, temperature);

    // place on screen base elements
    populate_base_ui();

    enum EpdDrawError err = epd_hl_update_screen(hl, MODE_EPDIY_WHITE_TO_GL16, temperature);

    if (err != EPD_DRAW_SUCCESS) {
        printf("Error updating screen: %d\n", err);
    }

    epd_poweroff();
    // vTaskDelay(1000);
}

void populate_base_ui(){
    // populate base UI
    // define font properties
    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_LEFT;

    // write Today's meeting string
    // TODO: include icon
    int cursor_x = 50;
    int cursor_y = 32;
    char meetings[] = "Today's Meetings";

    epd_write_string(font_11, meetings, &cursor_x, &cursor_y, fb, &font_props);

    // write date
    // TODO: make dynamic
    cursor_x = 52;
    cursor_y = 62;
    char date[] = "Monday, 1 Jan 2024";

    epd_write_string(font_9, date, &cursor_x, &cursor_y, fb, &font_props);

    // write weather
    // TODO: include icon
    cursor_x = 50+EPD_WIDTH/2;
    cursor_y = 32;
    char weather[] = "Weather";

    epd_write_string(font_11, weather, &cursor_x, &cursor_y, fb, &font_props);

    // write city
    // TODO: make dynamic based on config
    cursor_x = 52+EPD_WIDTH/2;
    cursor_y = 62;
    char city[] = "Lisbon, PT";

    epd_write_string(font_9, city, &cursor_x, &cursor_y, fb, &font_props);

    // draw center line
    epd_draw_vline(EPD_WIDTH/2, 0, EPD_HEIGHT, MID_GRAY, fb);
}