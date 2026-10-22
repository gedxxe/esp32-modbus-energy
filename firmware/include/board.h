#pragma once
// ESP32-S3 DevKitC-style N16R8; confirm labels on your exact board.
constexpr int PIN_SDA = 8, PIN_SCL = 9;
constexpr int PIN_RX = 18, PIN_TX = 17, PIN_RS485_DE = 16;
constexpr int PIN_PREV = 4, PIN_NEXT = 5, PIN_OK = 6, PIN_BUZZER = 7;
constexpr char AP_SSID[] = "Midea Engineering AP";
constexpr char AP_PASSWORD[] = "betahdijre12345";
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t LOCAL_OFFSET = 7 * 3600; // WIB. DS3231 stores UTC.
