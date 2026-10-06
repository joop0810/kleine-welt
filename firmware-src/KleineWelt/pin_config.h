#pragma once
// Pins der Waveshare ESP32-S3-Touch-AMOLED-1.75
// Quelle: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75

#define XPOWERS_CHIP_AXP2101

// AMOLED 466x466, CO5300 ueber QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Touch CST9217, AXP2101 und RTC teilen sich diesen I2C-Bus
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

// Spaeter: 6 Taster als 2x3-Matrix an der Stiftleiste (noch nicht benutzt)
#define BTN_ROW0 43
#define BTN_ROW1 44
#define BTN_COL0 16
#define BTN_COL1 17
#define BTN_COL2 18
