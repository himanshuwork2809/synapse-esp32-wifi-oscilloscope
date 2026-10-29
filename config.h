#ifndef CONFIG_H
#define CONFIG_H
#define I2C_PORT I2C_NUM_0
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQ 400000
#define OLED_ADDR 0x3C
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
// --- MCP3201 (SPI ADC) acquisition ---
#define ADC_SPI_HOST        SPI3_HOST   // VSPI_HOST was removed in newer ESP-IDF; VSPI = SPI3_HOST on ESP32
#define ADC_SPI_CLK_GPIO    14   // MCP3201 pin 7 CLK
#define ADC_SPI_MISO_GPIO   27   // MCP3201 pin 6 DOUT
#define ADC_SPI_CS_GPIO     13   // MCP3201 pin 5 CS
#define ADC_SPI_CLOCK_HZ    800000   // max SCLK at VDD=3.3V per MCP3201 datasheet
#define ADC_SAMPLE_RATE_HZ  50000
#define ADC_RING_BUFFER_LEN 4096  // must be power of two
#endif
