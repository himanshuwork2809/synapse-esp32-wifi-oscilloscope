#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "ui.h"
#include "esp_mac.h"
#include <string.h>
#include "http_server.h"
#include "tcp_server.h"
#include <stdbool.h>
static const char *TAG = "WIFI_MANAGER";
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    	if (event_base == WIFI_EVENT) {
        	switch (event_id) {
            		case WIFI_EVENT_AP_START:
    				ESP_LOGI(TAG, "SoftAP started successfully");
    				ui_show_ap_mode(wifi_manager_get_ap_ip());
    				break;
    			case WIFI_EVENT_AP_STACONNECTED: {wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    			ESP_LOGI(TAG,
         				"Station connected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d",
         				event->mac[0],
         				event->mac[1],
         				event->mac[2],
         				event->mac[3],
         				event->mac[4],
         				event->mac[5],
         				event->aid);
    				ui_show_pc_connected(wifi_manager_get_ap_ip());
    				http_server_start();
    				break;
			}
			case WIFI_EVENT_STA_START:
    				ESP_LOGI(TAG, "STA started");
    				break;
			case WIFI_EVENT_STA_DISCONNECTED: {
    				wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
    				ESP_LOGW(TAG, "STA disconnected, reason = %d", event->reason);
    				ui_show_wifi_failed();
    				break;
			}
            		default:
                		break;
        	}
    	}
    	if (event_base == IP_EVENT) {
    		if (event_id == IP_EVENT_STA_GOT_IP) {
        		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        		char ip[16];
        		snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        		ESP_LOGI(TAG, "Got IP: %s", ip);
        		ui_show_wifi_connected(ip);
        	tcp_server_start(ip);
    		}
	}
}
const char *wifi_manager_get_ap_ip(void) {
    	static char ip_string[16];
    	esp_netif_ip_info_t ip_info;
    	if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        	snprintf(ip_string, sizeof(ip_string), IPSTR, IP2STR(&ip_info.ip));
    	}
    	else {
        	strcpy(ip_string, "0.0.0.0");
    	}
    	return ip_string;
}
void wifi_manager_init(void) {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    		ESP_ERROR_CHECK(nvs_flash_erase());
    		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ap_netif = esp_netif_create_default_wifi_ap();
	sta_netif = esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	wifi_config_t ap_config = {
    		.ap = {
        		.ssid = "SYNAPSE",
        		.ssid_len = strlen("SYNAPSE"),
        		.password = "synapse123",
        		.channel = 1,
        		.max_connection = 4,
        		.authmode = WIFI_AUTH_OPEN
    		}
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "SoftAP started");
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,&wifi_event_handler, NULL, NULL));
} 
esp_err_t wifi_connect_sta(const char *ssid, const char *password){
	ui_show_connecting(ssid);
    	ESP_LOGI(TAG, "Stopping SoftAP");
    	ESP_ERROR_CHECK(esp_wifi_stop());
    	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    	wifi_config_t sta_config = {0};
    	strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    	strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    	ESP_ERROR_CHECK(esp_wifi_start());
    	ESP_LOGI(TAG, "Connecting to %s", ssid);
    	return esp_wifi_connect();
}
