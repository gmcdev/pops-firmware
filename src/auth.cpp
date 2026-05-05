#include "auth.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <time.h>

#include "config.h"

static char sessionToken[512] = {0};
static bool authenticated = false;

static void computeHmacSha256(
    const char* key,
    size_t keyLength,
    const char* message,
    size_t messageLength,
    uint8_t output[32]
) {
    mbedtls_md_context_t context;
    mbedtls_md_init(&context);
    mbedtls_md_setup(&context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&context, (const unsigned char*)key, keyLength);
    mbedtls_md_hmac_update(&context, (const unsigned char*)message, messageLength);
    mbedtls_md_hmac_finish(&context, output);
    mbedtls_md_free(&context);
}

static void bytesToHex(const uint8_t* bytes, size_t length, char* hexOut) {
    for (size_t index = 0; index < length; index++) {
        snprintf(hexOut + index * 2, 3, "%02x", bytes[index]);
    }
}

bool authAuthenticate() {
    const time_t timestamp = time(nullptr);

    // Refuse to authenticate if the clock has not been NTP-synced.
    // A stale timestamp would fail the backend's replay-window check.
    if (timestamp < 1000000000L) {
        #ifdef DEBUG
        Serial.println("[Auth] Clock not synced — skipping auth until NTP is ready");
        #endif
        return false;
    }

    // HMAC message: "DEVICE_ID:timestamp"
    char message[128];
    snprintf(message, sizeof(message), "%s:%ld", DEVICE_ID, (long)timestamp);

    uint8_t hmacBytes[32];
    computeHmacSha256(
        HARDWARE_SECRET, strlen(HARDWARE_SECRET),
        message, strlen(message),
        hmacBytes
    );

    char hmacHex[65] = {0};
    bytesToHex(hmacBytes, 32, hmacHex);

    // Build request body
    JsonDocument requestDoc;
    requestDoc["deviceId"] = DEVICE_ID;
    requestDoc["timestamp"] = (long)timestamp;

    String requestBody;
    serializeJson(requestDoc, requestBody);

    const String url = String(BACKEND_URL) + String(AUTH_ENDPOINT);

    // Both clients declared at function scope so their lifetime covers http.begin()
    // through http.end(). HTTPClient stores a reference internally — a block-scoped
    // client would be destroyed before POST() runs, causing a null pointer crash.
    // Production BACKEND_URL will be https:// — at that point, load and pin the
    // root CA cert rather than using setInsecure().
    WiFiClient plainClient;
    WiFiClientSecure secureClient;

    HTTPClient http;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, url);
    } else {
        http.begin(plainClient, url);
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-POPS-Signature", hmacHex);

    const int responseCode = http.POST(requestBody);

    #ifdef DEBUG
    Serial.printf("[Auth] POST %s => %d\n", url.c_str(), responseCode);
    #endif

    if (responseCode != 200) {
        http.end();
        authenticated = false;
        return false;
    }

    const String responseBody = http.getString();
    http.end();

    JsonDocument responseDoc;
    const DeserializationError parseError = deserializeJson(responseDoc, responseBody);

    if (parseError) {
        #ifdef DEBUG
        Serial.printf("[Auth] JSON parse error: %s\n", parseError.c_str());
        #endif
        authenticated = false;
        return false;
    }

    const char* token = responseDoc["token"];

    if (token == nullptr) {
        #ifdef DEBUG
        Serial.println("[Auth] Response missing 'token' field");
        #endif
        authenticated = false;
        return false;
    }

    strlcpy(sessionToken, token, sizeof(sessionToken));
    authenticated = true;

    #ifdef DEBUG
    Serial.println("[Auth] Authenticated successfully");
    #endif

    return true;
}

bool authIsAuthenticated() {
    return authenticated;
}

const char* authGetSessionToken() {
    return authenticated ? sessionToken : nullptr;
}
