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
#define PIN_RELAY      26
#define PIN_START_BTN  27

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
// WiFi reconnect backoff (milliseconds)
// ---------------------------------------------------------------------------
#define WIFI_RECONNECT_BASE_MS   1000
#define WIFI_RECONNECT_MAX_MS   30000

// ---------------------------------------------------------------------------
// API endpoints (paths only — prepend BACKEND_URL at call site)
// ---------------------------------------------------------------------------
#define AUTH_ENDPOINT "/api/devices/auth"
