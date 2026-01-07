#include "NFCReader.h"
#include "config.h"

NFCReader::NFCReader(uint8_t ssPin, uint8_t rstPin)
    : mfrc(ssPin, rstPin), ssPin(ssPin), rstPin(rstPin), lastReadTime(0) {
}

void NFCReader::begin() {
    mfrc.PCD_Init();
    delay(50);
    mfrc.PCD_SetAntennaGain(mfrc.RxGain_max);
    Serial.println("[NFC] RC522 initialized");
}

bool NFCReader::isCardPresent() {
    // Debug stats
    static unsigned long lastDebug = 0;
    static int callCount = 0;
    callCount++;
    unsigned long now = millis();
    if (now - lastDebug > 5000) {
        Serial.print("[NFC_DEBUG] is CardPresent called ");
        Serial.print(callCount);
        Serial.println(" times in 5s");
        callCount = 0;
        lastDebug = now;
    }
    
    // Cooldown between reads
    if (now - lastReadTime < CARD_READ_COOLDOWN_MS) {
        return false;
    }
    
    if (mfrc.PICC_IsNewCardPresent() && mfrc.PICC_ReadCardSerial()) {
        lastReadTime = now;
        return true;
    }
    
    return false;
}

bool NFCReader::reconnect() {
    // Reset card state
    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    
    // Wake up (WUPA)
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    
    // Reset ModWidth (standard formatting)
    mfrc.PCD_WriteRegister(mfrc.ModWidthReg, 0x26);
    
    MFRC522::StatusCode result = mfrc.PICC_WakeupA(bufferATQA, &bufferSize);
    
    if (result == MFRC522::STATUS_OK || result == MFRC522::STATUS_COLLISION) {
        // 4. Select (ReadCardSerial)
        if (mfrc.PICC_ReadCardSerial()) {
            return true;
        }
    }
    
    return false;
}

CardData NFCReader::readCard() {
    CardData card;
    card.has_card_id = false;
    card.has_credential = false;
    
    // Read hardware UID
    card.card_uid = uidToString(mfrc.uid);
    
    // Read card_id (blocks 4-6)
    String cardId = readNdefText(4, 3);
    if (cardId.length() > 0) {
        card.card_id = cardId;
        card.has_card_id = true;
    }
    
    // Small delay between reads to let card stabilize
    // This helps on first read after power-on when card may not be fully ready
    delay(50);
    
    // Read credential (30 blocks)
    String credentialRaw = readNdefText(8, 30);
    if (credentialRaw.length() > 0) {
        card.credential.raw = credentialRaw;
        card.credential.format = "jwt";
        card.has_credential = true;
    }
    
    // Leave card active for potential writes
    
    return card;
}

bool NFCReader::writeCardId(const String& cardId) {
    // Card should still be authenticated from readCard()
    // No need to re-select if user keeps card on reader
    
    bool success = writeNdefText(4, 3, cardId);
    
    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    
    return success;
}

bool NFCReader::clearCardId() {
    // Write empty string to clear card_id
    // This makes the card appear as blank for re-enrollment
    bool success = writeNdefText(4, 3, "");
    
    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    
    if (success) {
        Serial.println("[NFC] Card ID cleared - card is now blank");
    } else {
        Serial.println("[NFC] Failed to clear card ID");
    }
    return success;
}

bool NFCReader::clearCredential() {
    // Clear credential blocks (8-46)
    // Writing empty string will set length to 0
    bool success = writeNdefText(8, 30, "");
    
    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    
    Serial.println(success ? "[NFC] Credential cleared" : "[NFC] Failed to clear credential");
    return success;
}

bool NFCReader::writeCredential(const Credential& credential) {
    // Use 30 blocks (~478 chars)
    bool success = writeNdefText(8, 30, credential.raw);
    
    // Don't halt here - caller will halt after all operations
    // mfrc.PICC_HaltA();
    // mfrc.PCD_StopCrypto1();
    
    return success;
}

String NFCReader::getUID() {
    return uidToString(mfrc.uid);
}

void NFCReader::haltCard() {
    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    Serial.println("[NFC] Card halted and released");
}

String NFCReader::uidToString(const MFRC522::Uid& uid) {
    char buf[30] = {0};
    size_t p = 0;
    for (byte i = 0; i < uid.size; i++) {
        int n = snprintf(&buf[p], sizeof(buf) - p, "%02X", uid.uidByte[i]);  // No spaces
        if (n > 0) p += (size_t)n;
    }
    return String(buf);
}

String NFCReader::readNdefText(int startBlock, int numBlocks) {
    // Read text (skips trailers)
    
    Serial.print("[NFC_READ] Starting read at block ");
    Serial.print(startBlock);
    Serial.print(", numBlocks=");
    Serial.println(numBlocks);
    
    if (numBlocks > 30) numBlocks = 30;  // Safety limit increased
    
    byte blocks[30][18];  // Increased array size
    byte size = 18;
    
    // Authenticate with default key A
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
    
    // Read blocks, skipping trailer blocks (every 4th block: 7, 11, 15, 19, etc.)
    int blockIdx = 0;
    int currentBlock = startBlock;
    
    while (blockIdx < numBlocks) {
        // Skip trailer blocks (blocks 3, 7, 11, 15, 19, 23, ...)
        if ((currentBlock + 1) % 4 == 0) {
            currentBlock++;
            continue;
        }
        
        MFRC522::StatusCode status = mfrc.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc.uid));
        if (status != MFRC522::STATUS_OK) {
            return "";
        }
        
        size = 18;
        status = mfrc.MIFARE_Read(currentBlock, blocks[blockIdx], &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print("[NFC_READ] Read failed at block ");
            Serial.println(currentBlock);
            return "";
        }
        
        // Debug: show raw bytes
        Serial.print("[NFC_READ] Block ");
        Serial.print(currentBlock);
        Serial.print(": ");
        for (int j = 0; j < 16; j++) {
            if (blocks[blockIdx][j] < 0x10) Serial.print("0");
            Serial.print(blocks[blockIdx][j], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        blockIdx++;
        currentBlock++;
    }
    
    // Don't stop crypto - keep authentication for potential writes
    // mfrc.PCD_StopCrypto1();
    
    // Parse: first 2 bytes are length (little endian, supports up to 65535)
    int textLen = blocks[0][0] | (blocks[0][1] << 8);
    int maxLen = numBlocks * 16 - 2;  // -2 for length bytes
    
    Serial.print("[NFC_READ] Parsed length: ");
    Serial.print(textLen);
    Serial.print(" bytes (max=");
    Serial.print(maxLen);
    Serial.println(")");
    
    if (textLen == 0 || textLen > maxLen) {
        Serial.println("[NFC_READ] Invalid length, returning empty");
        return "";  // Invalid length
    }
    
    // Reconstruct text from blocks
    String text = "";
    text.reserve(textLen);  // Pre-allocate
    
    int read = 0;
    int byteIdx = 2;  // Start after 2-byte length
    
    for (int blk = 0; blk < numBlocks && read < textLen; blk++) {
        int startIdx = (blk == 0) ? 2 : 0;  // Skip length bytes in first block
        
        for (int i = startIdx; i < 16 && read < textLen; i++) {
            char c = (char)blocks[blk][i];
            if (c == 0) break;  // Stop at null terminator
            text += c;
            read++;
        }
    }
    
    Serial.print("[NFC_READ] Final text (");
    Serial.print(text.length());
    Serial.print(" chars): ");
    Serial.println(text);
    
    return text;
}

bool NFCReader::writeNdefText(int startBlock, int numBlocks, const String& text) {
    // Write text (skips trailers)
    
    Serial.print("[NFC_WRITE] Writing to block ");
    Serial.print(startBlock);
    Serial.print(", numBlocks=");
    Serial.print(numBlocks);
    Serial.print(", len=");
    Serial.println(text.length());
    
    if (numBlocks > 30) numBlocks = 30;  // Safety limit increased
    
    int textLen = text.length();
    int maxLen = numBlocks * 16 - 2;  // -2 for 2-byte length
    if (textLen > maxLen) {
        Serial.print("[NFC_WRITE] Text truncated from ");
        Serial.print(textLen);
        Serial.print(" to ");
        Serial.println(maxLen);
        textLen = maxLen;
    }
    
    // Authenticate with default key A
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
    
    // Prepare data across blocks
    byte blocks[30][16];  // Increased array size
    memset(blocks, 0, sizeof(blocks));
    
    // First block: [length_low][length_high][text...]
    blocks[0][0] = (byte)(textLen & 0xFF);  // Low byte
    blocks[0][1] = (byte)((textLen >> 8) & 0xFF);  // High byte
    
    int written = 0;
    // Fill first block (14 bytes available after 2-byte length)
    for (int i = 2; i < 16 && written < textLen; i++) {
        blocks[0][i] = text[written++];
    }
    
    // Fill remaining blocks
    for (int blk = 1; blk < numBlocks; blk++) {
        for (int i = 0; i < 16 && written < textLen; i++) {
            blocks[blk][i] = text[written++];
        }
    }
    
    // Write blocks, skipping trailer blocks
    int blockIdx = 0;
    int currentBlock = startBlock;
    
    while (blockIdx < numBlocks) {
        // Skip trailer blocks (blocks 3, 7, 11, 15, 19, 23, ...)
        if ((currentBlock + 1) % 4 == 0) {
            currentBlock++;
            continue;
        }
        
        MFRC522::StatusCode status = mfrc.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc.uid));
        if (status != MFRC522::STATUS_OK) {
            Serial.print("[NFC] Auth failed block ");
            Serial.print(currentBlock);
            Serial.print(": ");
            Serial.println(mfrc.GetStatusCodeName(status));
            return false;
        }
        
        status = mfrc.MIFARE_Write(currentBlock, blocks[blockIdx], 16);
        if (status != MFRC522::STATUS_OK) {
            Serial.print("[NFC_WRITE] Write failed block ");
            Serial.print(currentBlock);
            Serial.print(": ");
            Serial.println(mfrc.GetStatusCodeName(status));
            return false;
        }
        
        // Debug: show what was written
        Serial.print("[NFC_WRITE] Block ");
        Serial.print(currentBlock);
        Serial.print(": ");
        for (int j = 0; j < 16; j++) {
            if (blocks[blockIdx][j] < 0x10) Serial.print("0");
            Serial.print(blocks[blockIdx][j], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        blockIdx++;
        currentBlock++;
    }
    
    Serial.print("[NFC_WRITE] ✓ Success: ");
    Serial.print(numBlocks);
    Serial.print(" blocks at ");
    Serial.print(startBlock);
    Serial.print(", ");
    Serial.print(textLen);
    Serial.println(" chars written");
    
    return true;
}
