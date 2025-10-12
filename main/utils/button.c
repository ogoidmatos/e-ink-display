// ESP includes
#include "driver/gpio.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Own includes
#include "button.h"

// Static variables
static TaskHandle_t button_task_handle = NULL;
static volatile int button_pressed = -1;

// GPIO interrupt handler
// Only trigger on button release
static void button_isr_handler(void* args)
{
	static TickType_t last_press_time = 0;
	TickType_t current_time = xTaskGetTickCountFromISR();
	if (button_pressed == 0 && current_time - last_press_time > pdMS_TO_TICKS(50)) {
		button_pressed = 1;
		last_press_time = current_time;
		vTaskNotifyGiveFromISR(button_task_handle, NULL);
	} else if (button_pressed == -1 && current_time - last_press_time > pdMS_TO_TICKS(50)) {
		button_pressed = 0;
	}
}

// Dummy task for now to handle the button press
// Will later be used to switch between different UI screens
static void button_task(void* args)
{
	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if (button_pressed) {
			button_pressed = -1;
			printf("Button pressed\n");
		}
	}
}

void button_switch_context_init()
{
	gpio_reset_pin(BUTTON_PIN);
	gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

	gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);

	gpio_install_isr_service(0);

	xTaskCreate(button_task, "button handler", 2048, NULL, 1, &button_task_handle);
	gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, (void*)BUTTON_PIN);
}