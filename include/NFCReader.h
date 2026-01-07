#ifndef NFCREADER_H
#define NFCREADER_H

#include <Arduino.h>
#include <MFRC522.h>
#include "Models.h"

class NFCReader {
public:
    NFCReader(uint8_t ssPin, uint8_t rstPin);
    void begin();
    bool isCardPresent();
    bool reconnect();
    CardData readCard();
    bool writeCardId(const String& cardId);
    bool writeCredential(const Credential& credential);
    bool clearCardId();
    bool clearCredential();
    String getUID();
    void haltCard();
    
private:
    MFRC522 mfrc;
    uint8_t ssPin;
    uint8_t rstPin;
    uint32_t lastReadTime;
    
    String uidToString(const MFRC522::Uid& uid);
    String readNdefText(int startBlock, int numBlocks);
    bool writeNdefText(int startBlock, int numBlocks, const String& text);
};

#endif
