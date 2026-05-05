#include "comms.h"

#include <Arduino.h>

#include "config.h"

// Week 2: initialize PubSubClient pointed at HiveMQ Cloud broker.
// Subscribe to: devices/{DEVICE_ID}/commands
// Publish to:   devices/{DEVICE_ID}/status
// Use QoS 1 (at-least-once) for open_window commands.
// Reconnect loop must use exponential backoff — never spin-reconnect.
void commsSetup() {
    #ifdef DEBUG
    Serial.println("[Comms] Setup (stub — Week 2)");
    #endif
}

// Week 2: call mqttClient.loop(); reconnect with backoff if disconnected.
// Dispatch inbound open_window commands: validate HMAC + expires, then
// call relayPulse() and update display to WindowOpen.
// Reject open_window if a session is already active.
void commsLoop() {
}

bool commsIsConnected() {
    return false;
}
