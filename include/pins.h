#ifndef PINS_H
#define PINS_H

// ============================================
// ESP32-S3 N16R8 Pin Definitions
// Entry Hub - Display + Touch + Mic + LED
// ============================================

// ----- SPI Display (3.5" ILI9488) -----
#define TFT_MOSI    11      // SPI Master Out
#define TFT_MISO    13      // SPI Master In (optional)
#define TFT_SCLK    12      // SPI Clock
#define TFT_CS      10      // Chip Select
#define TFT_DC      8       // Data/Command
#define TFT_RST     9       // Reset
#define TFT_BL      46      // Backlight (PWM)

// ----- I2C Touch (FT6236) -----
#define TOUCH_SDA   38      // I2C Data
#define TOUCH_SCL   39      // I2C Clock
#define TOUCH_INT   40      // Interrupt (optional)
#define TOUCH_RST   41      // Reset (optional)

// ----- I2S Microphone (INMP441) -----
#define I2S_MIC_SCK     14  // Serial Clock (BCLK)
#define I2S_MIC_WS      15  // Word Select (LRCK)
#define I2S_MIC_SD      16  // Serial Data (DOUT)
#define I2S_MIC_PORT    I2S_NUM_0

// ----- I2S Speaker (MAX98357 - Optional) -----
#define I2S_SPK_BCLK    17  // Bit Clock
#define I2S_SPK_LRC     18  // Left/Right Clock
#define I2S_SPK_DIN     21  // Data In
#define I2S_SPK_PORT    I2S_NUM_1

// ----- Onboard RGB LED (ESP32-S3-DevKitC-1) -----
#define LED_PIN         48  // WS2812 RGB LED
#define LED_COUNT       1   // Single LED

// ----- I2C Bus (Shared) -----
#define I2C_SDA         TOUCH_SDA
#define I2C_SCL         TOUCH_SCL
#define I2C_FREQ        400000  // 400kHz

// ----- SPI Bus -----
#define SPI_FREQ        40000000  // 40MHz for display

#endif
