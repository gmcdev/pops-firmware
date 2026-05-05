#pragma once

// Configure relay and start button GPIO directions.
void relaySetup();

// Call from loop(). Monitors the start button and gates relay pulses on session state.
void relayLoop();

// Pulse the relay once (RELAY_PULSE_DURATION_MS on, RELAY_PULSE_GAP_MS off).
// Only callable when a session window is open. Week 2 implementation.
void relayPulse();
