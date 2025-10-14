// ESP includes
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"

// Own includes
#include "network_manager.h"

int connect_wifi()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	return 0;
}