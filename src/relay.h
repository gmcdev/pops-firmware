#pragma once

#include <stdint.h>

// Configure relay and start button GPIO directions.
void relaySetup();

// Call from loop(). Monitors the start button and gates relay pulses on session state.
void relayLoop();

// Pulse the relay once (RELAY_PULSE_DURATION_MS on, RELAY_PULSE_GAP_MS off).
void relayPulse();

// Pulse the relay count times, back to back, to dispense multiple credits.
void relayPulseN(uint8_t count);
