#include "acquisition.h"
#include "config.h"
#include "driver/spi_master.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static const char *TAG = "ACQ";
// --- ring buffer (single-producer/single-consumer, no lock needed) ---
static uint16_t ring_buf[ADC_RING_BUFFER_LEN];
static volatile uint32_t ring_head = 0; // written only by acquisition_task (producer)
static volatile uint32_t ring_tail = 0; // written only by the reader task (consumer)
static volatile uint32_t overrun_count = 0;
#define RING_MASK (ADC_RING_BUFFER_LEN - 1)
static spi_device_handle_t adc_spi;
static gptimer_handle_t sample_timer;
// --- measured sample rate (actual, not the configured target) ---
// Updated by acquisition_task every RATE_MEASURE_WINDOW samples from real
// elapsed gptimer time, so it tracks whatever rate the hardware/SPI/driver
// overhead actually achieves instead of assuming ADC_SAMPLE_RATE_HZ.
#define RATE_MEASURE_WINDOW 1000
static volatile float measured_sample_rate_hz = (float)ADC_SAMPLE_RATE_HZ; // seed with target until first window completes
static inline uint16_t mcp3201_read(void) {
    uint8_t rx[2] = {0, 0};
    spi_transaction_t t = {
        .length = 16,      // 16 clocks
        .rxlength = 16,
        .tx_buffer = NULL, // MCP3201 has no data input; driver clocks zeros
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(adc_spi, &t);
    uint16_t raw = ((uint16_t)rx[0] << 8) | rx[1];
    return (raw >> 1) & 0x0FFF; // drop trailing don't-care bit -> 12-bit code
}
static void acquisition_task(void *arg) {
    const uint64_t period_ticks = 1000000ULL / ADC_SAMPLE_RATE_HZ; // timer runs at 1MHz
    uint64_t now;
    gptimer_get_raw_count(sample_timer, &now);
    uint64_t next_tick = now + period_ticks;
    uint64_t window_start_us = now;
    uint32_t window_samples = 0;
    while (1) {
        do {
            gptimer_get_raw_count(sample_timer, &now);
        } while (now < next_tick);
        next_tick += period_ticks;
        uint16_t sample = mcp3201_read();
        uint32_t head = ring_head;
        uint32_t tail = ring_tail;
        if ((head - tail) >= ADC_RING_BUFFER_LEN) {
            // buffer full: consumer isn't keeping up, drop the sample
            overrun_count++;
            continue;
        }
        ring_buf[head & RING_MASK] = sample;
        ring_head = head + 1;
        // --- track the *actual* achieved rate over a rolling window ---
        // next_tick drifts behind "now" whenever a sample takes longer than
        // period_ticks, so the loop's own wall-clock time (not period_ticks)
        // is what tells us the real throughput.
        window_samples++;
        if (window_samples >= RATE_MEASURE_WINDOW) {
            uint64_t elapsed_us = now - window_start_us;
            if (elapsed_us > 0) {
                measured_sample_rate_hz =
                    (float)window_samples * 1000000.0f / (float)elapsed_us;
            }
            window_start_us = now;
            window_samples = 0;
            // This loop never blocks otherwise, which starves core 1's IDLE
            // task and trips the task watchdog (IDLE1 never gets to run and
            // reset it). One tick's worth of delay per window is a small,
            // bounded gap - it gets folded into the rate measurement above,
            // so the reported Sa/s stays honest.
            vTaskDelay(1);
            gptimer_get_raw_count(sample_timer, &now);
            next_tick = now + period_ticks;
        }
    }
}
esp_err_t acquisition_init(void) {
    esp_err_t err;
    spi_bus_config_t bus_cfg = {
        .miso_io_num = ADC_SPI_MISO_GPIO,
        .mosi_io_num = -1, // MCP3201 is read-only, no MOSI line
        .sclk_io_num = ADC_SPI_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4,
    };
    err = spi_bus_initialize(ADC_SPI_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %d", err);
        return err;
    }
    spi_device_interface_config_t dev_cfg = {
        .mode = 0, // MCP3201 supports SPI mode 0,0 or 1,1; mode 0 used here
        .clock_speed_hz = ADC_SPI_CLOCK_HZ,
        .spics_io_num = ADC_SPI_CS_GPIO,
        .queue_size = 1,
    };
    err = spi_bus_add_device(ADC_SPI_HOST, &dev_cfg, &adc_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %d", err);
        return err;
    }
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1 tick = 1us
    };
    err = gptimer_new_timer(&timer_cfg, &sample_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_new_timer failed: %d", err);
        return err;
    }
    err = gptimer_enable(sample_timer);
    if (err != ESP_OK) return err;
    err = gptimer_start(sample_timer);
    if (err != ESP_OK) return err;
    BaseType_t ok = xTaskCreatePinnedToCore(
        acquisition_task, "acq_task", 4096, NULL,
        configMAX_PRIORITIES - 2, NULL, 1 /* core 1 */);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create acquisition task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Acquisition started: target %d sps (period %llu us)",
             ADC_SAMPLE_RATE_HZ, (unsigned long long)(1000000ULL / ADC_SAMPLE_RATE_HZ));
    return ESP_OK;
}
size_t acquisition_read_samples(uint16_t *dst, size_t max_samples) {
    size_t count = 0;
    uint32_t tail = ring_tail;
    while (count < max_samples && tail != ring_head) {
        dst[count++] = ring_buf[tail & RING_MASK];
        tail++;
    }
    ring_tail = tail;
    return count;
}
size_t acquisition_available(void) {
    uint32_t head = ring_head;
    uint32_t tail = ring_tail;
    return (size_t)(head - tail);
}
uint32_t acquisition_get_overrun_count(void) {
    return overrun_count;
}
float acquisition_get_sample_rate_hz(void) {
    return measured_sample_rate_hz;
}
