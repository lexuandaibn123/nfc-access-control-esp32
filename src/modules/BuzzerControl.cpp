#include "BuzzerControl.h"
#include "config.h"

BuzzerControl::BuzzerControl(uint8_t pin, uint8_t channel)
    : pin(pin), channel(channel), alarmActive(false) {
}

void BuzzerControl::begin() {
    pinMode(pin, OUTPUT);
    ledcSetup(channel, 2000, 8);
    ledcAttachPin(pin, channel);
    off();
}

void BuzzerControl::accessGranted() {
    if (alarmActive) return;
    
    toneTimed(TONE_ACCESS_GRANTED_1, TONE_DURATION_SHORT);
    delay(TONE_DURATION_GAP);
    toneTimed(TONE_ACCESS_GRANTED_2, TONE_DURATION_SHORT);
}

void BuzzerControl::accessDenied() {
    if (alarmActive) return;
    toneTimed(TONE_ACCESS_DENIED, TONE_DURATION_MEDIUM);
}

void BuzzerControl::startAlarm() {
    if (alarmActive) return;
    alarmActive = true;
    tone(TONE_ALARM);
}

void BuzzerControl::stopAlarm() {
    if (!alarmActive) return;
    alarmActive = false;
    off();
}

bool BuzzerControl::isAlarmActive() const {
    return alarmActive;
}

void BuzzerControl::tone(uint32_t frequency) {
    if (frequency == 0) {
        off();
        return;
    }
    ledcWriteTone(channel, frequency);
    ledcWrite(channel, 220);
}

void BuzzerControl::toneTimed(uint32_t frequency, uint32_t durationMs) {
    if (alarmActive) return;
    
    tone(frequency);
    if (durationMs > 0) {
        delay(durationMs);
        off();
    }
}

void BuzzerControl::off() {
    ledcWriteTone(channel, 0);
}
