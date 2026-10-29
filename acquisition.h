#ifndef ACQUISITION_H
#define ACQUISITION_H
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
esp_err_t acquisition_init(void);
size_t acquisition_read_samples(uint16_t *dst, size_t max_samples);
size_t acquisition_available(void);
uint32_t acquisition_get_overrun_count(void);
// Actual measured sampling rate in Hz, averaged over a rolling window of
// real samples (see RATE_MEASURE_WINDOW in acquisition.c). Seeded with
// ADC_SAMPLE_RATE_HZ until the first measurement window completes.
float acquisition_get_sample_rate_hz(void);
#endif
