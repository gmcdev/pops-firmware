#include "comms.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "auth.h"
#include "config.h"
#include "display.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static WiFiClientSecure tlsClient;
static PubSubClient mqttClient(tlsClient);

static bool windowOpen = false;
static uint8_t windowCredits = 1;
static unsigned long windowOpenedAtMs = 0;
static char currentSessionId[64] = {0};

static unsigned long reconnectLastAttemptMs = 0;
static unsigned long reconnectIntervalMs = MQTT_RECONNECT_BASE_MS;

// ---------------------------------------------------------------------------
// MQTT message handler
// ---------------------------------------------------------------------------

static void onMessage(const char *topic, byte *payload, unsigned int length) {
    JsonDocument doc;
    const DeserializationError parseError = deserializeJson(doc, payload, length);
    if (parseError) {
        #ifdef DEBUG
        Serial.printf("[Comms] JSON parse error: %s\n", parseError.c_str());
        #endif
        return;
    }

    const char *command = doc["command"];
    if (command == nullptr) {
        return;
    }

    if (strcmp(command, "open_window") == 0) {
        if (windowOpen) {
            #ifdef DEBUG
            Serial.println("[Comms] open_window rejected — session already active");
            #endif
            return;
        }

        const char *sessionId = doc["sessionId"];
        if (sessionId == nullptr) {
            return;
        }

        strlcpy(currentSessionId, sessionId, sizeof(currentSessionId));
        windowOpen = true;
        windowOpenedAtMs = millis();

        const int creditsFromPayload = doc["credits"] | 1;
        windowCredits = (creditsFromPayload > 0 && creditsFromPayload <= 10)
            ? (uint8_t)creditsFromPayload
            : 1;

        displaySetState(DisplayState::WindowOpen);

        // Acknowledge — QoS 0 publish is sufficient; backend has its own timeout
        JsonDocument ackDoc;
        ackDoc["command"] = "ack";
        ackDoc["sessionId"] = sessionId;
        ackDoc["deviceId"] = DEVICE_ID;

        char ackPayload[128];
        serializeJson(ackDoc, ackPayload, sizeof(ackPayload));
        mqttClient.publish(MQTT_STATUS_TOPIC, ackPayload);

        #ifdef DEBUG
        Serial.printf("[Comms] Window open — session %s\n", sessionId);
        #endif
    }
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

static void connectMqtt() {
    // TODO: pin the HiveMQ CA certificate before production
    tlsClient.setInsecure();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(onMessage);
    mqttClient.setBufferSize(512);

    const String clientId = String("pops-") + String(DEVICE_ID);

    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        mqttClient.subscribe(MQTT_COMMAND_TOPIC, 1); // QoS 1
        reconnectIntervalMs = MQTT_RECONNECT_BASE_MS;

        #ifdef DEBUG
        Serial.printf("[Comms] MQTT connected — subscribing to %s\n", MQTT_COMMAND_TOPIC);
        #endif
    } else {
        // Exponential backoff
        reconnectIntervalMs = reconnectIntervalMs * 2;
        if (reconnectIntervalMs > MQTT_RECONNECT_MAX_MS) {
            reconnectIntervalMs = MQTT_RECONNECT_MAX_MS;
        }

        #ifdef DEBUG
        Serial.printf("[Comms] MQTT connect failed (rc=%d) — retry in %lums\n",
                      mqttClient.state(), reconnectIntervalMs);
        #endif
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void commsSetup() {
    #ifdef DEBUG
    Serial.println("[Comms] Setup complete");
    #endif
}

void commsLoop() {
    if (windowOpen && (millis() - windowOpenedAtMs >= WINDOW_EXPIRY_MS)) {
        windowOpen = false;
        windowCredits = 1;
        currentSessionId[0] = '\0';
        displaySetState(DisplayState::Idle);

        #ifdef DEBUG
        Serial.println("[Comms] Window expired — returning to idle");
        #endif
    }

    if (!authIsAuthenticated()) {
        return;
    }

    if (!mqttClient.connected()) {
        const unsigned long nowMs = millis();
        if (nowMs - reconnectLastAttemptMs >= reconnectIntervalMs) {
            reconnectLastAttemptMs = nowMs;
            connectMqtt();
        }
        return;
    }

    mqttClient.loop();
}

bool commsIsConnected() {
    return mqttClient.connected();
}

bool commsIsWindowOpen() {
    return windowOpen;
}

uint8_t commsGetWindowCredits() {
    return windowCredits;
}

void commsCloseWindow() {
    windowOpen = false;
    windowCredits = 1;
    currentSessionId[0] = '\0';
    displaySetState(DisplayState::GameStarted);
}

void commsLog(const char* message) {
    #ifdef DEBUG
    Serial.println(message);
    #endif
    if (!mqttClient.connected()) {
        return;
    }
    // Publish to a dedicated log topic so the device can be monitored
    // without a USB serial connection (e.g. when powered from the machine).
    char topic[80];
    snprintf(topic, sizeof(topic), "devices/%s/log", DEVICE_ID);
    mqttClient.publish(topic, message);
}
