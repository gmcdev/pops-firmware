#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "config.h"

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
static DisplayState currentState = DisplayState::Idle;

static const char* stateLabel(const DisplayState state) {
    switch (state) {
        case DisplayState::Idle:              return "Tap to Play";
        case DisplayState::Processing:        return "Processing...";
        case DisplayState::WindowOpen:        return "Window Open";
        case DisplayState::GameStarted:       return "Game Started";
        case DisplayState::WindowClosed:      return "Window Closed";
        case DisplayState::InsufficientFunds: return "No Credits";
        case DisplayState::Error:             return "Error";
        default:                              return "Unknown";
    }
}

static void render() {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    const char* label = stateLabel(currentState);

    int16_t boundX, boundY;
    uint16_t textWidth, textHeight;
    oled.getTextBounds(label, 0, 0, &boundX, &boundY, &textWidth, &textHeight);

    const int16_t cursorX = (OLED_WIDTH - (int16_t)textWidth) / 2;
    const int16_t cursorY = (OLED_HEIGHT - (int16_t)textHeight) / 2;
    oled.setCursor(cursorX, cursorY);
    oled.print(label);

    oled.display();
}

void displaySetup() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        #ifdef DEBUG
        Serial.println("[Display] SSD1306 init failed — check wiring and I2C address");
        #endif
        return;
    }

    oled.clearDisplay();
    oled.display();

    #ifdef DEBUG
    Serial.println("[Display] Boot state cycle");
    #endif

    // Cycle through every state on boot for visual hardware verification
    const DisplayState bootSequence[] = {
        DisplayState::Idle,
        DisplayState::Processing,
        DisplayState::WindowOpen,
        DisplayState::GameStarted,
        DisplayState::WindowClosed,
        DisplayState::InsufficientFunds,
        DisplayState::Error
    };

    for (const DisplayState state : bootSequence) {
        displaySetState(state);
        delay(500);
    }

    displaySetState(DisplayState::Idle);
}

void displaySetState(const DisplayState state) {
    currentState = state;
    render();
}

void displayUpdate() {
    // Reserved for animated states (e.g., a spinner during Processing).
    // States are rendered synchronously on transition — no redraw needed here yet.
}

DisplayState displayGetState() {
    return currentState;
}
