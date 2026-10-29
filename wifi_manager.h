#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include "esp_err.h"
void wifi_manager_init(void);
const char *wifi_manager_get_ap_ip(void);
esp_err_t wifi_connect_sta(const char *ssid, const char *password);
#endif
