#pragma once

// Initialize the MQTT client. Called after WiFi is up and auth is complete.
// Week 2: connect to HiveMQ Cloud, subscribe to devices/{DEVICE_ID}/commands.
void commsSetup();

// Call from loop(). Maintains MQTT connection with exponential backoff.
// Dispatches inbound commands (open_window) to the appropriate handler.
void commsLoop();

// Returns true when the MQTT broker connection is active.
bool commsIsConnected();
