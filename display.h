#ifndef DISPLAY_H
#define DISPLAY_H
#include "esp_err.h"
esp_err_t display_initialize(void);
void display_draw_char(uint8_t x, uint8_t y, char c);
void display_update(void);
void display_clear(void);
void display_draw_string(uint8_t x, uint8_t y, const char *str);
#endif
