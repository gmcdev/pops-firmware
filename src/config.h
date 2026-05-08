#pragma once

// ---------------------------------------------------------------------------
// Build-time environment variables
// Injected from .env.local by scripts/load_env.py at compile time.
// Never hardcode values here — add them to .env.local.
// ---------------------------------------------------------------------------
#ifndef WIFI_SSID
#  error "WIFI_SSID is not defined. Add WIFI_SSID=<value> to .env.local."
#endif

#ifndef WIFI_PASSWORD
#  error "WIFI_PASSWORD is not defined. Add WIFI_PASSWORD=<value> to .env.local."
#endif

#ifndef BACKEND_URL
#  error "BACKEND_URL is not defined. Add BACKEND_URL=<value> to .env.local."
#endif

#ifndef HARDWARE_SECRET
#  error "HARDWARE_SECRET is not defined. Add HARDWARE_SECRET=<value> to .env.local."
#endif

#ifndef DEVICE_ID
#  error "DEVICE_ID is not defined. Add DEVICE_ID=<value> to .env.local."
#endif

// ---------------------------------------------------------------------------
// Hardware pin assignments
// All pin references in source must use these constants — no inline numbers.
// ---------------------------------------------------------------------------

// PN532 NFC (SPI)
#define PIN_NFC_SS     5
#define PIN_NFC_SCK   18
#define PIN_NFC_MISO  19
#define PIN_NFC_MOSI  23

// SSD1306 OLED (I2C)
#define PIN_OLED_SDA  21
#define PIN_OLED_SCL  22

// Relay and start button
// PIN_RELAY: relay output (GPIO 26).
// PIN_START_COL: column wire via 33kΩ/47kΩ divider → GPIO 25 (digital interrupt).
// PIN_START_ADC: row wire via 33kΩ/47kΩ divider → GPIO 34 (ADC1_CH6, WiFi-safe).
//   Both require shared ground (ESP32 powered from machine 5V).
//
// Detection: column ISR fires on FALLING edge → high-priority FreeRTOS task
// delays 75μs (capacitive coupling spike decays) → reads row ADC.
//   Row at idle/strobe (3.21V): ADC ~2340 → above threshold → no detection.
//   Row with switch closed (~0.4V): ADC ~295 → below threshold → detected.
#define PIN_RELAY              26
#define PIN_START_COL          25
#define PIN_START_ADC          34
#define ADC_SWITCH_THRESHOLD   200

// ---------------------------------------------------------------------------
// OLED display
// ---------------------------------------------------------------------------
#define OLED_WIDTH      128
#define OLED_HEIGHT      64
#define OLED_RESET       -1
#define OLED_I2C_ADDR  0x3C

// ---------------------------------------------------------------------------
// NFC token lifetime
// Token TTL must be at least 2× the rotation interval so the incoming tag
// is always valid when the customer's phone reads it.
// ---------------------------------------------------------------------------
#define NFC_TOKEN_TTL_MS          8000
#define NFC_ROTATION_INTERVAL_MS  4000

// ---------------------------------------------------------------------------
// Relay timing (machine-specific — adjust per cabinet in this file)
// ---------------------------------------------------------------------------
#define RELAY_PULSE_DURATION_MS  100
#define RELAY_PULSE_GAP_MS       200

// ---------------------------------------------------------------------------
// Session window expiry
// If the player opens a session window but never presses start, the window
// closes automatically after this interval and the display returns to Idle.
// ---------------------------------------------------------------------------
#define WINDOW_EXPIRY_MS  60000

// ---------------------------------------------------------------------------
// WiFi reconnect backoff (milliseconds)
// ---------------------------------------------------------------------------
#define WIFI_RECONNECT_BASE_MS   1000
#define WIFI_RECONNECT_MAX_MS   30000

// ---------------------------------------------------------------------------
// MQTT broker
// ---------------------------------------------------------------------------
#ifndef MQTT_BROKER
#  error "MQTT_BROKER is not defined. Add MQTT_BROKER=<value> to .env.local."
#endif

#ifndef MQTT_USERNAME
#  error "MQTT_USERNAME is not defined. Add MQTT_USERNAME=<value> to .env.local."
#endif

#ifndef MQTT_PASSWORD
#  error "MQTT_PASSWORD is not defined. Add MQTT_PASSWORD=<value> to .env.local."
#endif

#define MQTT_PORT                8883
#define MQTT_COMMAND_TOPIC       "devices/" DEVICE_ID "/commands"
#define MQTT_STATUS_TOPIC        "devices/" DEVICE_ID "/status"
#define MQTT_RECONNECT_BASE_MS   2000
#define MQTT_RECONNECT_MAX_MS   60000

// ---------------------------------------------------------------------------
// API endpoints (paths only — prepend BACKEND_URL at call site)
// ---------------------------------------------------------------------------
#define AUTH_ENDPOINT "/api/devices/auth"
