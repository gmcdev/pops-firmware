#pragma once

#include <stdint.h>

// Initialize the MQTT client. Connection is established in commsLoop() once
// WiFi and device auth are ready.
void commsSetup();

// Call from loop(). Maintains MQTT connection with exponential backoff.
// Dispatches inbound commands (open_window) to the appropriate handler.
void commsLoop();

// Returns true when the MQTT broker connection is active.
bool commsIsConnected();

// Returns true when an open_window command has been received and the session
// window is active (i.e. a button press should trigger a relay pulse).
bool commsIsWindowOpen();

// Returns the number of credits authorised for the active session window.
// Only valid when commsIsWindowOpen() is true.
uint8_t commsGetWindowCredits();

// Closes the active session window and transitions the display to GameStarted.
// Call this after a successful relay pulse.
void commsCloseWindow();

// Publish a diagnostic log line to the MQTT log topic so the device can be
// monitored without a USB serial connection.
void commsLog(const char* message);
