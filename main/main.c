// Standard includes
#include <stdio.h>

// Own includes
#include "ui/ui.h"
#include "utils/button.h"
#include "utils/network_manager.h"

EpdiyHighlevelState hl;

void app_main(void)
{
	button_switch_context_init();

	// setup UI
	ui_init(&hl);
}