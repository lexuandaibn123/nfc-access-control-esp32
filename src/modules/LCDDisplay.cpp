#include "LCDDisplay.h"
#include <Wire.h>

// Vietnamese character mapping (UTF-8 to ASCII)
const struct { const char* vn; const char* ascii; } VN_MAP[] = {
    {"á","a"},{"à","a"},{"ả","a"},{"ã","a"},{"ạ","a"},
    {"ă","a"},{"ắ","a"},{"ằ","a"},{"ẳ","a"},{"ẵ","a"},{"ặ","a"},
    {"â","a"},{"ấ","a"},{"ầ","a"},{"ẩ","a"},{"ẫ","a"},{"ậ","a"},
    {"đ","d"},{"Đ","D"},
    {"é","e"},{"è","e"},{"ẻ","e"},{"ẽ","e"},{"ẹ","e"},
    {"ê","e"},{"ế","e"},{"ề","e"},{"ể","e"},{"ễ","e"},{"ệ","e"},
    {"í","i"},{"ì","i"},{"ỉ","i"},{"ĩ","i"},{"ị","i"},
    {"ó","o"},{"ò","o"},{"ỏ","o"},{"õ","o"},{"ọ","o"},
    {"ô","o"},{"ố","o"},{"ồ","o"},{"ổ","o"},{"ỗ","o"},{"ộ","o"},
    {"ơ","o"},{"ớ","o"},{"ờ","o"},{"ở","o"},{"ỡ","o"},{"ợ","o"},
    {"ú","u"},{"ù","u"},{"ủ","u"},{"ũ","u"},{"ụ","u"},
    {"ư","u"},{"ứ","u"},{"ừ","u"},{"ử","u"},{"ữ","u"},{"ự","u"},
    {"ý","y"},{"ỳ","y"},{"ỷ","y"},{"ỹ","y"},{"ỵ","y"},
    {"Á","A"},{"À","A"},{"Ả","A"},{"Ã","A"},{"Ạ","A"},
    {"Ă","A"},{"Ắ","A"},{"Ằ","A"},{"Ẳ","A"},{"Ẵ","A"},{"Ặ","A"},
    {"Â","A"},{"Ấ","A"},{"Ầ","A"},{"Ẩ","A"},{"Ẫ","A"},{"Ậ","A"},
    {"É","E"},{"È","E"},{"Ẻ","E"},{"Ẽ","E"},{"Ẹ","E"},
    {"Ê","E"},{"Ế","E"},{"Ề","E"},{"Ể","E"},{"Ễ","E"},{"Ệ","E"},
    {"Í","I"},{"Ì","I"},{"Ỉ","I"},{"Ĩ","I"},{"Ị","I"},
    {"Ó","O"},{"Ò","O"},{"Ỏ","O"},{"Õ","O"},{"Ọ","O"},
    {"Ô","O"},{"Ố","O"},{"Ồ","O"},{"Ổ","O"},{"Ỗ","O"},{"Ộ","O"},
    {"Ơ","O"},{"Ớ","O"},{"Ờ","O"},{"Ở","O"},{"Ỡ","O"},{"Ợ","O"},
    {"Ú","U"},{"Ù","U"},{"Ủ","U"},{"Ũ","U"},{"Ụ","U"},
    {"Ư","U"},{"Ứ","U"},{"Ừ","U"},{"Ử","U"},{"Ữ","U"},{"Ự","U"},
    {"Ý","Y"},{"Ỳ","Y"},{"Ỷ","Y"},{"Ỹ","Y"},{"Ỵ","Y"}
};

// Remove Vietnamese diacritics for LCD compatibility
String removeVietnameseDiacritics(const String& text) {
    String result = text;
    for (const auto& mapping : VN_MAP) {
        result.replace(mapping.vn, mapping.ascii);
    }
    return result;
}

LCDDisplay::LCDDisplay(uint8_t addr, uint8_t cols, uint8_t rows)
    : lcd(addr, cols, rows), cols(cols), rows(rows) {
}

void LCDDisplay::begin(uint8_t sda, uint8_t scl) {
    Wire.begin(sda, scl);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    cachedLine1 = "";
    cachedLine2 = "";
}

void LCDDisplay::show(const String& line1, const String& line2) {
    // Remove Vietnamese diacritics (LCD doesn't support UTF-8)
    String l1 = fit(removeVietnameseDiacritics(line1), cols);
    String l2 = fit(removeVietnameseDiacritics(line2), cols);
    
    // Only update if changed (prevent flickering)
    if (l1 == cachedLine1 && l2 == cachedLine2) {
        return;
    }
    
    cachedLine1 = l1;
    cachedLine2 = l2;
    
    lcd.setCursor(0, 0);
    lcd.print(cachedLine1);
    lcd.setCursor(0, 1);
    lcd.print(cachedLine2);
}

void LCDDisplay::clear() {
    lcd.clear();
    cachedLine1 = "";
    cachedLine2 = "";
}

String LCDDisplay::fit(const String& s, uint8_t maxLen) {
    if (s.length() >= maxLen) {
        return s.substring(0, maxLen);
    }
    String result = s;
    while (result.length() < maxLen) {
        result += ' ';
    }
    return result;
}
