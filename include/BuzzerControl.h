#ifndef BUZZERCONTROL_H
#define BUZZERCONTROL_H

#include <Arduino.h>

class BuzzerControl {
public:
    BuzzerControl(uint8_t pin, uint8_t channel);
    void begin();
    void accessGranted();
    void accessDenied();
    void startAlarm();
    void stopAlarm();
    bool isAlarmActive() const;
    void toneTimed(uint32_t frequency, uint32_t durationMs);
    
private:
    uint8_t pin;
    uint8_t channel;
    bool alarmActive;
    
    void tone(uint32_t frequency);
    void off();
};

#endif
