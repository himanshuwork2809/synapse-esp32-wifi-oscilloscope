#include "glcdfont.h"
#include "display.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_log.h"
#include <string.h>
static const char *TAG = "DISPLAY";
static i2c_master_bus_handle_t display_bus = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint8_t framebuffer[128 * 64 / 8];
esp_err_t display_initialize(void) {
	i2c_master_bus_config_t bus_config = {
    		.i2c_port = -1,                // Let ESP-IDF choose a free I2C controller
    		.sda_io_num = I2C_SDA,
    		.scl_io_num = I2C_SCL,
    		.clk_source = I2C_CLK_SRC_DEFAULT,
    		.glitch_ignore_cnt = 4,
    		.intr_priority = 0,
    		.trans_queue_depth = 0,
    		.flags = {
        		.enable_internal_pullup = true,
        		.allow_pd = false,
    		}
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_bus));
	esp_lcd_panel_io_i2c_config_t io_config = {
    		.dev_addr = OLED_ADDR,
    		.scl_speed_hz = I2C_FREQ,
    		.control_phase_bytes = 1,
    		.dc_bit_offset = 6,
    		.lcd_cmd_bits = 8,
    		.lcd_param_bits = 8,
    		.on_color_trans_done = NULL,
    		.user_ctx = NULL,
    		.flags = {
        		.dc_low_on_data = false,
        		.disable_control_phase = false,
    		}
	};
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_bus, &io_config, &io_handle));
	esp_lcd_panel_dev_config_t panel_config = {
 		.bits_per_pixel = 1,
    		.reset_gpio_num = GPIO_NUM_NC,
    		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    		.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    		.vendor_config = NULL,
    		.flags = {
        		.reset_active_high = false,
    		}
	};
	esp_err_t ret;
	ret = esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle);
	ESP_LOGI(TAG, "new_panel = %s", esp_err_to_name(ret));
	ESP_LOGI(TAG, "panel_handle = %p", panel_handle);
	ret = esp_lcd_panel_reset(panel_handle);
	ESP_LOGI(TAG, "panel_reset = %s", esp_err_to_name(ret));
	ret = esp_lcd_panel_init(panel_handle);
	ESP_LOGI(TAG, "panel_init = %s", esp_err_to_name(ret));
	ret = esp_lcd_panel_mirror(panel_handle, true, true);
	ESP_LOGI(TAG, "panel_mirror = %s", esp_err_to_name(ret));
	ret = esp_lcd_panel_disp_on_off(panel_handle, true);
	ESP_LOGI(TAG, "panel_on = %s", esp_err_to_name(ret));
	memset(framebuffer, 0xFF, sizeof(framebuffer));
	ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle,0, 0, 128, 64, framebuffer));
    	ESP_LOGI(TAG, "Display initialize");
    	return ESP_OK;
}
void display_draw_char(uint8_t x, uint8_t y, char c) {
	if (c < 32 || c > 126) {
		return;
    	}
    	const uint8_t *bitmap = &font[(uint8_t)c * 5];
    	for (uint8_t col = 0; col < 5; col++) {
		uint8_t line = bitmap[col];
        	for (uint8_t row = 0; row < 8; row++) {
        		if (line & (1 << row)) {
        			uint16_t px = x + col;
				uint16_t py = y + row;
				framebuffer[px + (py / 8) * 128] |= (1 << (py % 8));
			}
		}
    	}
}
void display_draw_string(uint8_t x, uint8_t y, const char *str) {
    	while (*str) {
        	display_draw_char(x, y, *str);
        	x += 6;      // Move to the next character position
        	str++;       // Move to the next character in the string
    	}
}
void display_update(void) {
    	esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, framebuffer);
}
void display_clear(void) {
    	memset(framebuffer, 0, sizeof(framebuffer));
}

