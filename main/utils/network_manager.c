// ESP includes
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"

// Own includes
#include "network_manager.h"

static EventGroupHandle_t wifi_event_group;
static uint8_t retry_num = 0;

static void wifi_event_handler(void* arg,
							   esp_event_base_t event_base,
							   int32_t event_id,
							   void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
		ESP_LOGD(LOG_TAG_NETWORK, "Attempting to connect to WiFi.");
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (retry_num < MAXIMUM_RETRY) {
			esp_wifi_connect();
			ESP_LOGD(LOG_TAG_NETWORK, "Retrying to connect to WiFi.");
			retry_num++;
		} else {
			xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
		}
	}
}

static void ip_event_handler(void* arg,
							 esp_event_base_t event_base,
							 int32_t event_id,
							 void* event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
		ESP_LOGD(LOG_TAG_NETWORK, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
		retry_num = 0;
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
	static int output_len; // Stores number of bytes read
	switch (evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			ESP_LOGD(LOG_TAG_HTTP,
					 "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
					 evt->header_key,
					 evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			// Clean the buffer in case of a new request
			if (output_len == 0 && evt->user_data) {
				// we are just starting to copy the output data into the use
				memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
			}
			// If user_data buffer is configured, copy the response into the buffer
			int copy_len = 0;
			if (evt->user_data) {
				// The last byte in evt->user_data is kept for the NULL character in case of
				// out-of-bound access.
				copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
				if (copy_len) {
					memcpy(evt->user_data + output_len, evt->data, copy_len);
				}
			} else {
				ESP_LOGE(LOG_TAG_HTTP, "Response buffer is null.");
			}
			output_len += copy_len;

			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_FINISH");
			output_len = 0;
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
			output_len = 0;
			break;
		case HTTP_EVENT_REDIRECT:
			ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_REDIRECT");
			break;
	}
	return ESP_OK;
}

uint8_t connect_wifi()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_event_group = xEventGroupCreate();

	esp_event_handler_instance_t wifi_handler_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
	  WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_handler_event_instance));

	esp_event_handler_instance_t got_ip_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
	  IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, &got_ip_event_instance));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

	// set the wifi controller to be a station
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	// set the wifi config
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

	// start the wifi driver
	ESP_ERROR_CHECK(esp_wifi_start());

	EventBits_t bits = xEventGroupWaitBits(
	  wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGD(LOG_TAG_NETWORK, "Connected to ap");
		return 0;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGD(LOG_TAG_NETWORK, "Failed to connect to ap");
		return 1;
	} else {
		ESP_LOGE(LOG_TAG_NETWORK, "Unexpected behaviour connecting to the wifi.");
		return 1;
	}
}

uint8_t https_get_request(const char* url, char* output_buffer)
{
	esp_http_client_config_t config = {
		.url = url,
		.event_handler = http_event_handler,
		.user_data = output_buffer, // Pass the buffer to get response
		.skip_cert_common_name_check = true,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);

	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGD(LOG_TAG_HTTP,
				 "HTTPS GET Status = %d, content_length = %d",
				 esp_http_client_get_status_code(client),
				 esp_http_client_get_content_length(client));
	} else {
		ESP_LOGE(LOG_TAG_HTTP, "HTTPS GET request failed: %s", esp_err_to_name(err));
	}

	esp_http_client_cleanup(client);
	return err == ESP_OK ? 0 : 1;
}