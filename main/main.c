#include <stdio.h>

#include "ui/ui.h"
#include "utils/button.h"

EpdiyHighlevelState hl;

void app_main(void)
{
	button_switch_context_init();

	// setup UI
	ui_init(&hl);
}