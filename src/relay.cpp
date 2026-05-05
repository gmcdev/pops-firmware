#include "relay.h"

#include <Arduino.h>

#include "config.h"

void relaySetup() {
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    pinMode(PIN_START_BTN, INPUT_PULLUP);

    #ifdef DEBUG
    Serial.println("[Relay] Setup complete");
    #endif
}

// Week 2: read PIN_START_BTN; if pressed and a session window is open, call relayPulse().
// If pressed without an open window, update display to InsufficientFunds.
void relayLoop() {
}

// Week 2: drive PIN_RELAY HIGH for RELAY_PULSE_DURATION_MS, then LOW.
// Gap between pulses is RELAY_PULSE_GAP_MS. Per-machine timing is in config.h.
void relayPulse() {
}
