#ifndef DOORSENSOR_H
#define DOORSENSOR_H

#include <Arduino.h>

class DoorSensor {
public:
    DoorSensor(uint8_t pin, int openLevel);
    void begin();
    bool isOpen();
    
private:
    uint8_t pin;
    int openLevel;
    uint32_t lastChangeTime;
    bool lastState;
    bool stableState;
};

#endif
