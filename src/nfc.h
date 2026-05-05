#pragma once

// Initialize the PN532 over SPI.
// Week 2: load first token and start the emulation loop.
void nfcSetup();

// Call from loop(). Rotates the NFC token every NFC_ROTATION_INTERVAL_MS.
void nfcLoop();
