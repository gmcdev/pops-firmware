#include <Arduino.h>

#include "auth.h"
#include "comms.h"
#include "config.h"
#include "display.h"
#include "nfc.h"
#include "relay.h"
#include "network.h"

void setup() {
    #ifdef DEBUG
    Serial.begin(115200);
    Serial.println("[Main] POPS firmware booting");
    #endif

    // Display first so the OLED shows the boot state cycle during hardware init
    displaySetup();

    relaySetup();
    nfcSetup();
    commsSetup();

    // WiFi last — blocks briefly on first connect and triggers NTP sync
    wifiSetup();
}

void loop() {
    wifiMaintain();

    if (wifiIsConnected() && !authIsAuthenticated()) {
        authAuthenticate();
    }

    commsLoop();
    nfcLoop();
    relayLoop();
    displayUpdate();
}
