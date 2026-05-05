#include "network.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

static unsigned long reconnectLastAttemptMs = 0;
static unsigned long reconnectIntervalMs = WIFI_RECONNECT_BASE_MS;

static void syncNtp() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    #ifdef DEBUG
    Serial.print("[WiFi] Waiting for NTP sync");
    #endif

    struct tm timeInfo = {};
    int attempts = 0;
    const int maxAttempts = 20;

    while (!getLocalTime(&timeInfo) && attempts < maxAttempts) {
        #ifdef DEBUG
        Serial.print(".");
        #endif
        delay(500);
        attempts++;
    }

    #ifdef DEBUG
    if (attempts < maxAttempts) {
        char timeBuf[64];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S UTC", &timeInfo);
        Serial.printf("\n[WiFi] NTP synced: %s\n", timeBuf);
    } else {
        Serial.println("\n[WiFi] NTP sync failed — auth will be skipped until clock is valid");
    }
    #endif
}

static void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    #ifdef DEBUG
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    #endif

    const unsigned long timeoutMs = 15000;
    const unsigned long startMs = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs) {
        #ifdef DEBUG
        Serial.print(".");
        #endif
        delay(500);
    }

    #ifdef DEBUG
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Connection timed out");
    }
    #endif

    if (WiFi.status() == WL_CONNECTED) {
        syncNtp();
    }
}

void wifiSetup() {
    connectWifi();
    reconnectLastAttemptMs = millis();
    reconnectIntervalMs = WIFI_RECONNECT_BASE_MS;
}

void wifiMaintain() {
    if (WiFi.status() == WL_CONNECTED) {
        reconnectIntervalMs = WIFI_RECONNECT_BASE_MS;
        return;
    }

    const unsigned long nowMs = millis();
    if (nowMs - reconnectLastAttemptMs < reconnectIntervalMs) {
        return;
    }

    #ifdef DEBUG
    Serial.printf("[WiFi] Reconnecting (backoff %lums)...\n", reconnectIntervalMs);
    #endif

    reconnectLastAttemptMs = nowMs;
    reconnectIntervalMs = min(reconnectIntervalMs * 2, (unsigned long)WIFI_RECONNECT_MAX_MS);

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}
