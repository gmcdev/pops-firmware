#include "relay.h"

#include <Arduino.h>
#include <driver/adc.h>

#include "comms.h"
#include "config.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Start button detection — column-timed ADC sampling
//
// Column wire: 33kΩ/47kΩ divider → GPIO 25 (FALLING interrupt).
// Row wire:    33kΩ/47kΩ divider → GPIO 34 (ADC1_CH6, WiFi-safe).
// Shared ground required: ESP32 powered from machine 5V.
//
// ISR fires on each column scan FALLING edge. After a 2ms debounce (to
// suppress ringing) and 75μs delay (coupling spike decays), the row ADC
// is read directly in the ISR:
//   Row at strobe/idle (~3.2V): ADC ~2340 → above threshold → no detection.
//   Row with switch closed (~0.4V): ADC ~295 → below threshold → detected.
// adc1_get_raw() is safe in ISR for ADC1 while WiFi uses ADC2.
// ---------------------------------------------------------------------------

static volatile bool startButtonFlag = false;
static volatile uint32_t detectionCount = 0;
static uint16_t adcIdleLevel = 0;
static unsigned long lastPressMs = 0;
static const unsigned long DEBOUNCE_DELAY_MS = 200;

static volatile uint32_t windowDetectionCount = 0;
static unsigned long windowStartMs = 0;
static const uint32_t WINDOW_DETECTION_THRESHOLD = 3;
static const unsigned long WINDOW_MS = 100;

static volatile uint32_t isrFireCount = 0;
static volatile int isrLastRaw = -1;

static void IRAM_ATTR onColumnFall() {
    static uint32_t lastIsrUs = 0;
    const uint32_t nowUs = micros();
    if (nowUs - lastIsrUs < 2000) {
        return;
    }
    lastIsrUs = nowUs;
    isrFireCount++;

    // No delay — sample immediately at the falling edge while column is LOW.
    // The ADC conversion itself takes ~5μs and integrates over the scan window.
    const int raw = adc1_get_raw(ADC1_CHANNEL_6);
    isrLastRaw = raw;
    if (raw >= 0 && static_cast<uint16_t>(raw) < ADC_SWITCH_THRESHOLD) {
        detectionCount++;
        windowDetectionCount++;
        if (windowDetectionCount >= WINDOW_DETECTION_THRESHOLD) {
            startButtonFlag = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void relaySetup() {
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
    adc1_config_width(ADC_WIDTH_BIT_12);

    uint32_t sum = 0;
    for (uint8_t i = 0; i < 32; i++) {
        sum += adc1_get_raw(ADC1_CHANNEL_6);
        delay(1);
    }
    adcIdleLevel = static_cast<uint16_t>(sum / 32);

    pinMode(PIN_START_COL, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_START_COL), onColumnFall, FALLING);

    char msg[64];
    snprintf(msg, sizeof(msg), "[Relay] Setup complete — ADC idle: %u", adcIdleLevel);
    commsLog(msg);
}

void relayPulse() {
    commsLog("[Relay] Pulsing relay");
    digitalWrite(PIN_RELAY, HIGH);
    delay(RELAY_PULSE_DURATION_MS);
    digitalWrite(PIN_RELAY, LOW);
    delay(RELAY_PULSE_GAP_MS);
}

void relayPulseN(const uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        relayPulse();
    }
}

void relayLoop() {
    const unsigned long nowMs = millis();

    if (nowMs - windowStartMs >= WINDOW_MS) {
        windowDetectionCount = 0;
        windowStartMs = nowMs;
    }

    #ifdef DEBUG
    static unsigned long lastDiagMs = 0;
    if (nowMs - lastDiagMs >= 2000) {
        lastDiagMs = nowMs;
        const uint32_t dets = detectionCount;
        const uint32_t isrFires = isrFireCount;
        detectionCount = 0;
        isrFireCount = 0;
        const int adcRaw = adc1_get_raw(ADC1_CHANNEL_6);
        char msg[96];
        snprintf(msg, sizeof(msg), "[Relay] adcIdle=%u adcNow=%d isrRaw=%d det/2s=%u isr/2s=%u",
            adcIdleLevel, adcRaw, isrLastRaw, dets, isrFires);
        commsLog(msg);
    }
    #endif

    if (!startButtonFlag) {
        return;
    }

    const unsigned long pressNowMs = millis();
    if (pressNowMs - lastPressMs < DEBOUNCE_DELAY_MS) {
        startButtonFlag = false;
        return;
    }

    startButtonFlag = false;
    lastPressMs = pressNowMs;

    if (commsIsWindowOpen()) {
        relayPulseN(commsGetWindowCredits());
        commsCloseWindow();
    } else {
        commsLog("[Relay] Start pressed with no open window");
        displaySetState(DisplayState::InsufficientFunds);
    }
}
