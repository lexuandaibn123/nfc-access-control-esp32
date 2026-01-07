#include "ButtonControl.h"
#include "config.h"

ButtonControl::ButtonControl(uint8_t pin, bool activeLow)
    : pin(pin), activeLow(activeLow), lastRaw(true), stableState(true), 
      changeTime(0), lastPressTime(0) {
}

void ButtonControl::begin() {
    pinMode(pin, INPUT_PULLUP);
    lastRaw = digitalRead(pin);
    stableState = lastRaw;
}

bool ButtonControl::wasPressed() {
    uint32_t now = millis();
    bool currentRaw = digitalRead(pin);
    
    // Detect change
    if (currentRaw != lastRaw) {
        lastRaw = currentRaw;
        changeTime = now;
    }
    
    // Debounce
    if (now - changeTime >= BUTTON_DEBOUNCE_MS) {
        if (currentRaw != stableState) {
            stableState = currentRaw;
            
            // Check if this is a press event
            bool pressed = activeLow ? (stableState == LOW) : (stableState == HIGH);
            
            if (pressed && (now - lastPressTime >= BUTTON_COOLDOWN_MS)) {
                lastPressTime = now;
                return true;
            }
        }
    }
    
    return false;
}
