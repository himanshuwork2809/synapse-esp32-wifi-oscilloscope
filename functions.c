#include "functions.h"
static const char *I2C_TAG = "I2C";
static const char *OLED_TAG = "OLED";
static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t oled;
esp_err_t i2c_initialize(void) {
	i2c_master_bus_config_t cfg={
		.i2c_port=I2C_PORT,
		.sda_io_num=I2C_SDA,
		.scl_io_num=I2C_SCL,
		.clk_source=I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt=7,
		.flags.enable_internal_pullup=true
	};
	esp_err_t err=i2c_new_master_bus(&cfg,&i2c_bus);
	if(err==ESP_OK)
		ESP_LOGI(I2C_TAG,"I2C Initialized");
	else
		ESP_LOGE(I2C_TAG,"I2C Init Failed");
	return err;
}
