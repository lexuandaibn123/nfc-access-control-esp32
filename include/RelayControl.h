#ifndef RELAYCONTROL_H
#define RELAYCONTROL_H

#include <Arduino.h>

class RelayControl {
public:
    RelayControl(uint8_t pin, bool activeLow = false);
    void begin();
    void unlock();
    void lock();
    bool isUnlocked() const;
    
private:
    uint8_t pin;
    bool activeLow;
    bool unlocked;
    
    void setState(bool on);
};

#endif
