#ifndef UI_H
#define UI_H

#include "epd_highlevel.h"

#define BLACK 0x00
#define WHITE 0xFF
#define MID_GRAY 0x8C

#define TEMPERATURE 22

static uint8_t* fb;

void ui_init(EpdiyHighlevelState* hl);

void populate_base_ui();

#endif // UI_H