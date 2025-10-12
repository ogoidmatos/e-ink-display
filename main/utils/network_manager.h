#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

// include necessary for static assert
#include <string.h>

// WiFi credentials
// Use values from menuconfig, translated to sdkconfig variables
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD

// Compile-time assertions to ensure that the user has set their WiFi credentials
_Static_assert(strcmp(WIFI_SSID, "your-ssid") != 0, "Please configure WiFi credentials in menuconfig");
_Static_assert(strcmp(WIFI_PASSWORD, "your-password") != 0, "Please configure WiFi credentials in menuconfig");

#endif // NETWORK_MANAGER_H