#include "nfc.h"

#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <SPI.h>
#include <mbedtls/md.h>
#include <time.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------

// Thin subclass that promotes private Adafruit_PN532 methods needed for our
// custom TgInitAsTarget implementation without modifying the library.
class PN532Extended : public Adafruit_PN532 {
public:
    PN532Extended(uint8_t cs, SPIClass *spi) : Adafruit_PN532(cs, spi) {}
    bool waitReady(uint16_t timeout) { return waitready(timeout); }
    void readData(uint8_t *buff, uint8_t n) { readdata(buff, n); }
};

static SPIClass nfcSpi(VSPI);
static PN532Extended pn532(PIN_NFC_SS, &nfcSpi);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static unsigned long lastRotationMs = 0;
static bool isInitialized = false;

// NDEF file buffer — rebuilt on every token rotation
static uint8_t ndefBuffer[256];
static uint16_t ndefLength = 0;

// Currently selected file in the APDU state machine (0 = none)
static uint16_t selectedFileId = 0;

// ---------------------------------------------------------------------------
// HMAC helpers
// Duplicated from auth.cpp — avoids a shared library dependency.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Token generation
// ---------------------------------------------------------------------------

// Builds the NFC deep-link URL into urlOut.
//
// Format: https://peaceofpinball.com/play?machine={DEVICE_ID}&expires={expiresMs}&token={hmac}
//
// The mobile app validates `expires` client-side before calling the backend.
// The backend verifies the HMAC using the device's hardware secret.
// HMAC message: "{DEVICE_ID}:{expiresMs}"
static void buildTokenUrl(char* urlOut, size_t urlOutSize) {
    const unsigned long expiresMs =
        (unsigned long)time(nullptr) * 1000UL + (unsigned long)NFC_TOKEN_TTL_MS;

    char message[128];
    snprintf(message, sizeof(message), "%s:%lu", DEVICE_ID, expiresMs);

    uint8_t hmacBytes[32];
    computeHmacSha256(
        HARDWARE_SECRET, strlen(HARDWARE_SECRET),
        message, strlen(message),
        hmacBytes
    );

    char hmacHex[65] = {0};
    bytesToHex(hmacBytes, 32, hmacHex);

    snprintf(
        urlOut, urlOutSize,
        "https://peaceofpinball.com/play?machine=%s&expires=%lu&token=%s",
        DEVICE_ID, expiresMs, hmacHex
    );
}

// ---------------------------------------------------------------------------
// NDEF construction
// ---------------------------------------------------------------------------

// Capability Container file per NFC Forum Type 4 Tag spec v2.0.
static const uint8_t ccFile[] = {
    0x00, 0x0F, // CC file length: 15 bytes
    0x20,       // Mapping version 2.0
    0x00, 0xF0, // Max MLe (ReadBinary response): 240 bytes
    0x00, 0xF0, // Max MLc (UpdateBinary command): 240 bytes
    0x04,       // NDEF File Control TLV tag
    0x06,       // TLV length: 6 bytes
    0xE1, 0x04, // NDEF File ID
    0x07, 0xFF, // Max NDEF data size: 2047 bytes
    0x00,       // NDEF read access: open
    0x00        // NDEF write access: open
};

// Constructs an NDEF URI record for the given HTTPS URL and writes it into
// buffer with a 2-byte length prefix as required by the Type 4 NDEF file format.
// Returns total bytes written, or 0 if the URL is too long for the buffer.
//
// URI identifier code 0x04 encodes the "https://" scheme prefix, so the
// payload carries only the URL body after that prefix.
static uint16_t buildNdefMessage(const char* url, uint8_t* buffer, uint16_t bufferSize) {
    const char* urlBody = (strncmp(url, "https://", 8) == 0) ? url + 8 : url;
    const size_t urlBodyLength = strlen(urlBody);

    // 1-byte URI code + URL body
    const uint16_t payloadLength = 1 + (uint16_t)urlBodyLength;

    // Short Record (SR=1): header(1) + typeLength(1) + payloadLength(1) + type(1) = 4 bytes overhead
    // SR is valid here because our payload fits in one byte (≤255). See URL length analysis in PLAN.md.
    const uint16_t recordLength = 4 + payloadLength;
    const uint16_t totalLength = 2 + recordLength; // 2-byte NDEF length prefix

    if (totalLength > bufferSize) {
        return 0;
    }

    uint16_t offset = 0;

    // NDEF file length prefix (big-endian, value = recordLength, not including itself)
    buffer[offset++] = (uint8_t)(recordLength >> 8);
    buffer[offset++] = (uint8_t)(recordLength & 0xFF);

    // NDEF record: MB=1, ME=1, CF=0, SR=1, IL=0, TNF=0x01 (Well-Known)
    buffer[offset++] = 0xD1;
    buffer[offset++] = 0x01; // Type length: 1 byte
    buffer[offset++] = (uint8_t)payloadLength;
    buffer[offset++] = 'U';  // URI Well-Known Type
    buffer[offset++] = 0x04; // URI identifier: "https://"

    memcpy(buffer + offset, urlBody, urlBodyLength);
    offset += (uint16_t)urlBodyLength;

    return offset;
}

// ---------------------------------------------------------------------------
// Low-level target data exchange
// ---------------------------------------------------------------------------
// The Adafruit library's getDataTarget / setDataTarget have two problems:
//   1. readdata() is called without waitReady() first, so it reads garbage
//      if the reader hasn't sent its next APDU yet.
//   2. setDataTarget requires cmd[0] == 0x8E (TgSetData opcode) — the caller
//      must prepend it, but the library comment about this is easy to miss.
// These wrappers fix both.

static bool getDataFromTarget(uint8_t *cmd, uint8_t *cmdlen) {
    uint8_t txBuf[1] = {0x86}; // TgGetData
    if (!pn532.sendCommandCheckAck(txBuf, 1, 1000)) {
        return false;
    }
    if (!pn532.waitReady(1000)) {
        return false;
    }
    uint8_t rxBuf[72] = {0};
    pn532.readData(rxBuf, sizeof(rxBuf));

    // Response: [preamble][0x00][0xFF][LEN][LCS][0xD5][0x87][Tg][data...]
    // data_length = LEN - 3 (TFI + CMD + Tg)
    if (rxBuf[3] < 3) {
        *cmdlen = 0;
        return false;
    }
    const uint8_t dataLength = rxBuf[3] - 3;
    for (uint8_t index = 0; index < dataLength; index++) {
        cmd[index] = rxBuf[8 + index];
    }
    *cmdlen = dataLength;
    return (dataLength > 0);
}

static bool sendDataToTarget(const uint8_t *data, uint8_t dataLength) {
    // TgSetData: first byte must be the 0x8E command opcode, data follows
    uint8_t txBuffer[270];
    txBuffer[0] = 0x8E;
    memcpy(txBuffer + 1, data, dataLength);

    if (!pn532.sendCommandCheckAck(txBuffer, dataLength + 1, 1000)) {
        return false;
    }
    if (!pn532.waitReady(500)) {
        return false;
    }
    uint8_t rxBuf[8] = {0};
    pn532.readData(rxBuf, sizeof(rxBuf));
    return true;
}

// ---------------------------------------------------------------------------
// APDU state machine
// ---------------------------------------------------------------------------
// Implements the NFC Forum Type 4 Tag command set:
//   SELECT Application (AID = D2 76 00 00 85 01 01)
//   SELECT File        (0xE103 = CC, 0xE104 = NDEF)
//   READ BINARY        (served from ccFile or ndefBuffer)
//
// NOTE: Validate against physical hardware before production. Key things to
// confirm: ATQA/SAK byte order accepted by iPhone, AID bytes, CC file format.
// ---------------------------------------------------------------------------

static bool handleApdu() {
    uint8_t commandBuffer[64];
    uint8_t commandLength = 0;

    if (!getDataFromTarget(commandBuffer, &commandLength)) {
        #ifdef DEBUG
        Serial.println("[NFC] getDataFromTarget failed");
        #endif
        return false;
    }

    #ifdef DEBUG
    Serial.printf("[NFC] APDU (%d bytes): %02X %02X %02X %02X\n",
                  commandLength,
                  commandLength > 0 ? commandBuffer[0] : 0,
                  commandLength > 1 ? commandBuffer[1] : 0,
                  commandLength > 2 ? commandBuffer[2] : 0,
                  commandLength > 3 ? commandBuffer[3] : 0);
    #endif

    if (commandLength < 4) {
        return false;
    }

    const uint8_t cla = commandBuffer[0];
    const uint8_t ins = commandBuffer[1];
    const uint8_t p1  = commandBuffer[2];
    const uint8_t p2  = commandBuffer[3];

    uint8_t response[265];
    uint16_t responseLength = 0;

    if (cla == 0x00 && ins == 0xA4) {
        // SELECT command
        if (p1 == 0x04 && commandLength >= 7) {
            // SELECT Application by AID
            static const uint8_t ndefAid[] = { 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01 };
            const uint8_t lc = commandBuffer[4];
            if (lc == sizeof(ndefAid) && memcmp(commandBuffer + 5, ndefAid, sizeof(ndefAid)) == 0) {
                selectedFileId = 0;
                response[responseLength++] = 0x90;
                response[responseLength++] = 0x00; // OK
            } else {
                response[responseLength++] = 0x6A;
                response[responseLength++] = 0x82; // File/application not found
            }
        } else if (p1 == 0x00 && p2 == 0x0C && commandLength >= 7) {
            // SELECT File by ID
            const uint16_t fileId = ((uint16_t)commandBuffer[5] << 8) | commandBuffer[6];
            if (fileId == 0xE103 || fileId == 0xE104) {
                selectedFileId = fileId;
                response[responseLength++] = 0x90;
                response[responseLength++] = 0x00;
            } else {
                response[responseLength++] = 0x6A;
                response[responseLength++] = 0x82;
            }
        } else {
            response[responseLength++] = 0x6A;
            response[responseLength++] = 0x82;
        }
    } else if (cla == 0x00 && ins == 0xB0) {
        // READ BINARY
        const uint16_t readOffset = ((uint16_t)p1 << 8) | p2;
        const uint8_t le = (commandLength > 4) ? commandBuffer[4] : 0;

        const uint8_t* fileData = nullptr;
        uint16_t fileSize = 0;

        if (selectedFileId == 0xE103) {
            fileData = ccFile;
            fileSize = (uint16_t)sizeof(ccFile);
        } else if (selectedFileId == 0xE104) {
            fileData = ndefBuffer;
            fileSize = ndefLength;
        }

        if (fileData == nullptr || readOffset >= fileSize) {
            response[responseLength++] = 0x6B;
            response[responseLength++] = 0x00; // Wrong parameters
        } else {
            const uint16_t bytesAvailable = fileSize - readOffset;
            const uint16_t bytesToRead = (le == 0 || le > bytesAvailable) ? bytesAvailable : le;
            memcpy(response, fileData + readOffset, bytesToRead);
            responseLength = bytesToRead;
            response[responseLength++] = 0x90;
            response[responseLength++] = 0x00;
        }
    } else {
        response[responseLength++] = 0x6D;
        response[responseLength++] = 0x00; // Instruction not supported
    }

    sendDataToTarget(response, (uint8_t)responseLength);
    return true;
}

// ---------------------------------------------------------------------------
// Token rotation
// ---------------------------------------------------------------------------

static void rotateToken() {
    char url[512];
    buildTokenUrl(url, sizeof(url));
    ndefLength = buildNdefMessage(url, ndefBuffer, sizeof(ndefBuffer));
    selectedFileId = 0; // Reset APDU state — new token, new exchange

    #ifdef DEBUG
    if (ndefLength > 0) {
        Serial.printf("[NFC] Token rotated — NDEF: %d bytes, TTL: %dms\n",
                      ndefLength, NFC_TOKEN_TTL_MS);
        Serial.printf("[NFC] URL: %s\n", url);
    } else {
        Serial.println("[NFC] NDEF build failed — URL too long for buffer");
    }
    #endif
}

// ---------------------------------------------------------------------------
// Tag emulation
// ---------------------------------------------------------------------------
// Replaces the library's AsTarget() which has two problems:
//   1. SAK=0x60 has the UID-cascade bit set, which confuses iPhone — correct
//      value for ISO14443-4 emulation is SAK=0x20 (UID complete).
//   2. readdata() in newer Adafruit BusIO builds reads immediately without
//      waiting for PN532 ready, so it returns garbage and the function exits
//      before any reader has a chance to activate the tag.
// This version calls waitready() explicitly before reading the response.

static bool initAsTarget() {
    uint8_t command[] = {
        0x8C,                                                       // TgInitAsTarget
        0x00,                                                       // Mode: passive only
        0x04, 0x00,                                                 // SENS_RES (ATQA)
        0xDC, 0x44, 0x20,                                           // NFCID1t (3 bytes)
        0x20,                                                       // SEL_RES (SAK): ISO14443-4, UID complete
        0x01, 0xFE, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,           // FeliCa NFCID2
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,           // Pad
        0xFF, 0xFF,                                                 // SystemCode
        0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // NFCID3t
        0x00,                                                       // GtLength
        0x00                                                        // TkLength
    };

    if (!pn532.sendCommandCheckAck(command, sizeof(command), 1000)) {
        return false;
    }

    // Block until the PN532 signals it has been activated by a reader.
    // timeout=5ms: short so the main loop cycles frequently for relay ADC
    // sampling. nfcLoop() is called again immediately on the next iteration.
    if (!pn532.waitReady(5)) {
        return false;
    }

    uint8_t response[32] = {0};
    pn532.readData(response, 32);

    #ifdef DEBUG
    Serial.printf("[NFC] initAsTarget response: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  response[0], response[1], response[2], response[3],
                  response[4], response[5], response[6], response[7]);
    #endif

    // Valid TgInitAsTarget response: TFI=0xD5, CMD=0x8D
    return (response[5] == 0xD5 && response[6] == 0x8D);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void nfcSetup() {
    nfcSpi.begin(PIN_NFC_SCK, PIN_NFC_MISO, PIN_NFC_MOSI, PIN_NFC_SS);
    pn532.begin();

    const uint32_t firmwareVersion = pn532.getFirmwareVersion();
    if (firmwareVersion == 0) {
        #ifdef DEBUG
        Serial.println("[NFC] PN532 not found — check SPI wiring");
        #endif
        return;
    }

    #ifdef DEBUG
    Serial.printf("[NFC] PN532 ready: chip=PN5%02X fw=%d.%d\n",
                  (firmwareVersion >> 24) & 0xFF,
                  (firmwareVersion >> 16) & 0xFF,
                  (firmwareVersion >> 8) & 0xFF);
    #endif

    pn532.SAMConfig();
    isInitialized = true;
}

void nfcLoop() {
    if (!isInitialized) {
        return;
    }

    const unsigned long nowMs = millis();

    // Require a valid NTP-synced clock before generating tokens.
    // time() returns near-zero until wifiSetup() completes NTP sync.
    if (nowMs - lastRotationMs >= NFC_ROTATION_INTERVAL_MS && time(nullptr) >= 1000000000L) {
        rotateToken();
        lastRotationMs = nowMs;
    }

    // No token ready yet — wait for NTP sync
    if (ndefLength == 0) {
        return;
    }

    if (!initAsTarget()) {
        return;
    }

    // Reader (iPhone) connected — run the APDU exchange
    #ifdef DEBUG
    Serial.println("[NFC] Reader connected");
    #endif

    selectedFileId = 0;

    for (uint8_t exchangesRemaining = 20; exchangesRemaining > 0; exchangesRemaining--) {
        if (!handleApdu()) {
            break;
        }
    }

    #ifdef DEBUG
    Serial.println("[NFC] Reader left");
    #endif
}
