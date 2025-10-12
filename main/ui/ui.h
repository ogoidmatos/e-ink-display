#ifndef UI_H
#define UI_H

// EPD driver includes
#include "epd_highlevel.h"

#define BLACK 0x00
#define WHITE 0xFF
#define MID_GRAY 0x8C

#define TEMPERATURE 22

#define LOG_TAG_UI "UI"

int ui_init(EpdiyHighlevelState* hl);

int populate_base_ui();

#endif // UI_H