#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "ui.h"
#include <string.h>
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static bool json_get_string(const char *json, const char *key, char *value, size_t value_size) {
    	char pattern[32];
    	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    	char *p = strstr(json, pattern);
    	if (p == NULL)
        	return false;
    	p += strlen(pattern);
    	while (*p && *p != ':')
        	p++;
    	if (*p != ':')
        	return false;
	p++;
    	while (*p == ' ' || *p == '\t')
        	p++;
    	if (*p != '"')
        	return false;
    	p++;
    	char *end = strchr(p, '"');
    	if (end == NULL)
        	return false;
    	size_t len = end - p;
    	if (len >= value_size)
        	len = value_size - 1;
    	memcpy(value, p, len);
    	value[len] = '\0';
    	return true;
}
static const char *TAG = "HTTP_SERVER";
static char wifi_ssid[33];
static char wifi_password[65];
//GET
static esp_err_t root_get_handler(httpd_req_t *req) {
    	ESP_LOGI(TAG, "GET / received");
    	const char response[] = "SYNAPSE Online";
    	httpd_resp_set_type(req, "text/plain");
   	esp_err_t err = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    	ESP_LOGI(TAG, "httpd_resp_send() returned %d", err);
    	return err;
}
static void deferred_connect_task(void *arg) {
    	vTaskDelay(pdMS_TO_TICKS(500)); // give the "OK" response time to flush over the AP first
    	wifi_connect_sta(wifi_ssid, wifi_password);
    	vTaskDelete(NULL);
}
//POST
static esp_err_t wifi_post_handler(httpd_req_t *req) {
    	char buffer[256];
    	int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    	if (received <= 0) {
        	httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
        	return ESP_FAIL;
    	}
    	buffer[received] = '\0';
    	char ssid[33];
    	char password[65];
    	if (!json_get_string(buffer, "ssid", ssid, sizeof(ssid)) || !json_get_string(buffer, "password", password, sizeof(password))) {
        	httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        	return ESP_FAIL;
    	}
    	ESP_LOGI(TAG, "SSID     : %s", ssid);
    	ESP_LOGI(TAG, "Password : %s", password);
    	ui_show_received_credentials(ssid, password);
    	strcpy(wifi_ssid, ssid);
	strcpy(wifi_password, password);
    	httpd_resp_set_type(req, "text/plain");
	httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
	xTaskCreate(deferred_connect_task, "wifi_connect", 4096, NULL, 5, NULL);
	return ESP_OK;
}
void http_server_start(void) {
    	static httpd_handle_t server = NULL;
    	if (server != NULL) {
        	ESP_LOGI(TAG, "HTTP Server already running");
        	return;
    	}
    	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    	httpd_uri_t root = {
        	.uri = "/",
        	.method = HTTP_GET,
        	.handler = root_get_handler,
        	.user_ctx = NULL
    	};
    	httpd_uri_t wifi = {
        	.uri = "/wifi",
        	.method = HTTP_POST,
        	.handler = wifi_post_handler,
        	.user_ctx = NULL
    	};
    	esp_err_t err = httpd_start(&server, &config);
    	if (err == ESP_OK) {
        	httpd_register_uri_handler(server, &root);
        	httpd_register_uri_handler(server, &wifi);
        	ESP_LOGI(TAG, "HTTP Server Started");
    	}
    	else {
        	ESP_LOGE(TAG, "Failed to start HTTP Server");
    	}
}
