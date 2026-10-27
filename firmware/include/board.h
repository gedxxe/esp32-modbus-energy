#pragma once
// ESP32-S3 DevKitC-style N16R8; confirm labels on your exact board.
constexpr int PIN_SDA = 8, PIN_SCL = 9;
constexpr int PIN_RX = 18, PIN_TX = 17, PIN_RS485_DE = 16;
constexpr int PIN_PREV = 4, PIN_NEXT = 5, PIN_OK = 6, PIN_BUZZER = 7;
constexpr char AP_SSID[] = "Midea Engineering AP";
constexpr char AP_PASSWORD[] = "betahdijre12345";
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t LOCAL_OFFSET = 7 * 3600; // WIB. DS3231 stores UTC.

// One setting rotates every OLED screen, including startup and storage recovery.
constexpr bool OLED_ROTATE_180 = true;
// Stage 0 artwork is held before the existing loading animation.
constexpr uint32_t OLED_BOOT_SPLASH_MS = 3000;
static_assert(OLED_BOOT_SPLASH_MS<=10000,"Keep boot artwork hold within 10 seconds");
// Onboard addressable RGB (3 channels). This installation uses GPIO48; DevKitC v1.1 revisions may use GPIO38.
constexpr bool RGB_LED_ENABLED = true;
constexpr uint8_t RGB_LED_PIN = 48; // Physical GPIO, not the RGB_BUILTIN alias.
constexpr uint8_t RGB_LED_DEFAULT_BRIGHTNESS = 128; // 0..255; web can override.
constexpr uint32_t RGB_LED_DEFAULT_PERIOD_MS = 8000;
static_assert(RGB_LED_PIN==48||RGB_LED_PIN==38,"Select the onboard LED GPIO: 48 or 38");
static_assert(RGB_LED_DEFAULT_PERIOD_MS>=500&&RGB_LED_DEFAULT_PERIOD_MS<=30000,"LED cycle must be 500..30000 ms");

// Confirmed installation: manual module, GPIO16 drives tied DE and /RE.
// Connection settings can explicitly select the automatic-module alternative.
constexpr bool RS485_AUTO_DIRECTION = false;
