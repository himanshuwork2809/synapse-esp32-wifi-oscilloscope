#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "config.h"
esp_err_t i2c_initialize(void);
esp_err_t oled_initialize(void);
esp_err_t oled_command(uint8_t cmd);
esp_err_t oled_data(const uint8_t *data, size_t len);
#endif
