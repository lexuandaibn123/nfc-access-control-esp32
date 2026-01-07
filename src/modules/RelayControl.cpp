#include "RelayControl.h"

RelayControl::RelayControl(uint8_t pin, bool activeLow)
    : pin(pin), activeLow(activeLow), unlocked(false) {
}

void RelayControl::begin() {
    pinMode(pin, OUTPUT);
    lock();  // Start locked
}

void RelayControl::unlock() {
    setState(true);
    unlocked = true;
}

void RelayControl::lock() {
    setState(false);
    unlocked = false;
}

bool RelayControl::isUnlocked() const {
    return unlocked;
}

void RelayControl::setState(bool on) {
    bool level = on ? (activeLow ? LOW : HIGH) : (activeLow ? HIGH : LOW);
    digitalWrite(pin, level);
}
