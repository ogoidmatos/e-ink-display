#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

// System includes
#include <stdint.h>
#include <string.h>

// WiFi credentials
// Use values from menuconfig, translated to sdkconfig variables
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD

// Compile-time assertions to ensure that the user has set their WiFi credentials
_Static_assert(strcmp(WIFI_SSID, "your-ssid") != 0, "Please configure WiFi credentials in menuconfig");
_Static_assert(strcmp(WIFI_PASSWORD, "your-password") != 0, "Please configure WiFi credentials in menuconfig");

// API configuration
#define WEATHER_API_KEY CONFIG_WEATHER_API_KEY

#if !defined (CONFIG_USE_DYNAMIC_LOCATION) || (CONFIG_USE_DYNAMIC_LOCATION == 0)

#define LATITUDE CONFIG_LATITUDE
#define LONGITUDE CONFIG_LONGITUDE

_Static_assert(strcmp(LATITUDE, "0.0") != 0, "Dynamic location is disabled. Please configure a valid latitude in menuconfig");
_Static_assert(strcmp(LONGITUDE, "0.0") != 0, "Dynamic location is disabled. Please configure a valid longitude in menuconfig");

#endif // CONFIG_USE_DYNAMIC_LOCATION

#define MAXIMUM_RETRY 10
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define LOG_TAG_NETWORK "NETWORK_MANAGER"
#define LOG_TAG_HTTP    "HTTP_CLIENT"

#define MAX_HTTP_OUTPUT_BUFFER 2048

#define MIN(a, b) ((a) < (b) ? (a) : (b))

uint8_t connect_wifi();
uint8_t https_get_request(const char* url, char* output_buffer);
void disconnect_wifi();

#endif // NETWORK_MANAGER_H