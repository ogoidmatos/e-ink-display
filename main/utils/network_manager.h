#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

// System includes
#include <stdint.h>
#include <string.h>

// WiFi credentials
// Use values from menuconfig, translated to sdkconfig variables
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD

#define MAXIMUM_RETRY 10
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define LOG_TAG_NETWORK "NETWORK_MANAGER"
#define LOG_TAG_HTTP    "HTTP_CLIENT"

#define MAX_HTTP_OUTPUT_BUFFER 2048

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Compile-time assertions to ensure that the user has set their WiFi credentials
_Static_assert(strcmp(WIFI_SSID, "your-ssid") != 0, "Please configure WiFi credentials in menuconfig");
_Static_assert(strcmp(WIFI_PASSWORD, "your-password") != 0, "Please configure WiFi credentials in menuconfig");

uint8_t connect_wifi();
uint8_t https_get_request(const char* url, char* output_buffer);

#endif // NETWORK_MANAGER_H