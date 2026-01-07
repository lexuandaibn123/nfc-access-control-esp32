#include "DoorSensor.h"
#include "config.h"

DoorSensor::DoorSensor(uint8_t pin, int openLevel)
    : pin(pin), openLevel(openLevel), lastChangeTime(0), lastState(false), stableState(false) {
}

void DoorSensor::begin() {
    pinMode(pin, INPUT_PULLUP);
    stableState = (digitalRead(pin) == openLevel);
    lastState = stableState;
}

bool DoorSensor::isOpen() {
    bool currentReading = (digitalRead(pin) == openLevel);
    uint32_t now = millis();
    
    // Detect state change
    if (currentReading != lastState) {
        lastChangeTime = now;
        lastState = currentReading;
    }
    
    // Debounce
    if (now - lastChangeTime >= DOOR_DEBOUNCE_MS) {
        stableState = currentReading;
    }
    
    return stableState;
}
