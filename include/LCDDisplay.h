#ifndef LCDISPLAY_H
#define LCDISPLAY_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

class LCDDisplay {
public:
    LCDDisplay(uint8_t addr, uint8_t cols, uint8_t rows);
    void begin(uint8_t sda, uint8_t scl);
    void show(const String& line1, const String& line2);
    void clear();
    
private:
    LiquidCrystal_I2C lcd;
    uint8_t cols;
    uint8_t rows;
    String cachedLine1;
    String cachedLine2;
    
    String fit(const String& s, uint8_t maxLen);
};

#endif
