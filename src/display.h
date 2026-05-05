#pragma once

#include <stdint.h>

enum class DisplayState : uint8_t {
    Idle,
    Processing,
    WindowOpen,
    GameStarted,
    WindowClosed,
    InsufficientFunds,
    Error
};

// Initialize the SSD1306 over I2C. Cycles through all states for visual boot test.
void displaySetup();

// Transition to a new state and render immediately.
void displaySetState(DisplayState state);

// Call from loop(). Reserved for animated states (e.g., spinner).
void displayUpdate();

// Returns the current display state.
DisplayState displayGetState();
