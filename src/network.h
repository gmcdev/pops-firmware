#pragma once

// Initialize WiFi and attempt first connection. Blocks up to ~15 seconds.
// Triggers NTP sync on successful connection.
void wifiSetup();

// Call from loop(). Reconnects with exponential backoff when disconnected.
void wifiMaintain();

// Returns true when the WiFi station link is up.
bool wifiIsConnected();
