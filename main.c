#include "functions.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"
#include "wifi_manager.h"
void app_main(void) {
    	display_initialize();
	wifi_manager_init();
    	ui_show_ap_mode("192.168.4.1");
    	while (1) {
        	vTaskDelay(pdMS_TO_TICKS(1000));
    	}
}
