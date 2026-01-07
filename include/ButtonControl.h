#ifndef BUTTONCONTROL_H
#define BUTTONCONTROL_H

#include <Arduino.h>

class ButtonControl {
public:
    ButtonControl(uint8_t pin, bool activeLow);
    void begin();
    bool wasPressed();
    
private:
    uint8_t pin;
    bool activeLow;
    bool lastRaw;
    bool stableState;
    uint32_t changeTime;
    uint32_t lastPressTime;
};

#endif
