#include "JWTVerifier.h"
#include <Ed25519.h>
#include <SHA512.h>

JWTVerifier::JWTVerifier() {
}

bool JWTVerifier::verify(const String& jwt, const String& publicKeyPem, JWTPayload& outPayload) {
    // JWT format: header.payload.signature
    int firstDot = jwt.indexOf('.');
    int secondDot = jwt.indexOf('.', firstDot + 1);
    
    if (firstDot == -1 || secondDot == -1) {
        Serial.println("[JWT] Invalid JWT format");
        return false;
    }
    
    String header = jwt.substring(0, firstDot);
    String payload = jwt.substring(firstDot + 1, secondDot);
    String signature = jwt.substring(secondDot + 1);
    
    // Message to verify: header.payload
    String message = jwt.substring(0, secondDot);
    
    // Verify signature
    if (!verifyEdDSASignature(message, signature, publicKeyPem)) {
        Serial.println("[JWT] Signature verification failed");
        return false;
    }
    
    // Decode and parse payload
    uint8_t payloadBytes[512];
    int payloadLen = base64UrlDecode(payload, payloadBytes, 512);
    if (payloadLen == 0) {
        Serial.println("[JWT] Failed to decode payload");
        return false;
    }
    
    String payloadJson = String((char*)payloadBytes).substring(0, payloadLen);
    if (!parsePayload(payloadJson, outPayload)) {
        Serial.println("[JWT] Failed to parse payload");
        return false;
    }
    
    Serial.println("[JWT] Verification successful");
    return true;
}

int JWTVerifier::base64UrlDecode(const String& input, uint8_t* output, int maxLen) {
    // Convert base64url to base64
    String base64 = input;
    base64.replace('-', '+');
    base64.replace('_', '/');
    
    // Add padding
    while (base64.length() % 4 != 0) {
        base64 += '=';
    }
    
    Serial.print("[JWT] Base64 input length: ");
    Serial.println(input.length());
    Serial.print("[JWT] After padding: ");
    Serial.println(base64.length());
    
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    int decodedLen = 0;
    
    // Process 4 characters at a time
    for (size_t i = 0; i < base64.length(); i += 4) {
        uint32_t value = 0;
        int validChars = 0;
        
        // Decode 4 base64 chars to up to 3 bytes
        for (int j = 0; j < 4; j++) {
            if (i + j >= base64.length()) break;
            
            char c = base64[i + j];
            if (c == '=') {
                break;  // Stop at padding
            }
            
            const char* pos = strchr(b64, c);
            if (!pos) continue;
            
            value = (value << 6) | (pos - b64);
            validChars++;
        }
        
        // Extract bytes based on valid chars (not padding)
        if (validChars >= 2) {
            if (decodedLen < maxLen) output[decodedLen++] = (value >> (validChars * 6 - 8)) & 0xFF;
        }
        if (validChars >= 3) {
            if (decodedLen < maxLen) output[decodedLen++] = (value >> (validChars * 6 - 16)) & 0xFF;
        }
        if (validChars >= 4) {
            if (decodedLen < maxLen) output[decodedLen++] = (value >> (validChars * 6 - 24)) & 0xFF;
        }
    }
    
    Serial.print("[JWT] Decoded length: ");
    Serial.println(decodedLen);
    
    return decodedLen;
}

bool JWTVerifier::verifyEdDSASignature(const String& message, const String& signatureB64, const String& publicKeyPem) {
    // Parse Ed25519 public key from PEM
    String keyData = publicKeyPem;
    keyData.replace("-----BEGIN PUBLIC KEY-----", "");
    keyData.replace("-----END PUBLIC KEY-----", "");
    keyData.replace("\n", "");
    keyData.replace("\r", "");
    keyData.trim();
    
    // Decode public key (SPKI format - DER encoded)
    uint8_t decoded[128];
    int decodedLen = base64UrlDecode(keyData, decoded, 128);
    
    if (decodedLen < 32) {
        Serial.print("[JWT] Decoded key too short: ");
        Serial.print(decodedLen);
        Serial.println(" bytes");
        return false;
    }
    
    // Find Ed25519 key in SPKI: look for 0x03 0x21 0x00 pattern
    int keyOffset = -1;
    for (int i = 0; i < decodedLen - 34; i++) {  // Need i+2 for pattern, i+35 for full key
        if (decoded[i] == 0x03 && decoded[i+1] == 0x21 && decoded[i+2] == 0x00) {
            keyOffset = i + 3;
            break;
        }
    }
    
    if (keyOffset == -1 || (decodedLen - keyOffset) < 32) {
        Serial.println("[JWT] Could not find Ed25519 key in SPKI structure");
        Serial.print("[JWT] Decoded data (");
        Serial.print(decodedLen);
        Serial.print(" bytes): ");
        for (int i = 0; i < min(20, decodedLen); i++) {
            Serial.print(decoded[i], HEX);
            Serial.print(" ");
        }
        Serial.println("...");
        return false;
    }
    
    // Extract 32-byte public key
    uint8_t publicKey[32];
    for (int i = 0; i < 32; i++) {
        publicKey[i] = decoded[keyOffset + i];
    }
    
    Serial.println("[JWT] Ed25519 public key extracted successfully");
    
    // Decode signature
    uint8_t signatureBytes[64];
    int sigLen = base64UrlDecode(signatureB64, signatureBytes, 64);
    if (sigLen != 64) {
        Serial.print("[JWT] Invalid signature length: ");
        Serial.println(sigLen);
        return false;
    }
    
    // Verify with Ed25519
    bool valid = Ed25519::verify(signatureBytes, publicKey, (const uint8_t*)message.c_str(), message.length());
    
    if (!valid) {
        Serial.println("[JWT] Ed25519 verification failed");
    } else {
        Serial.println("[JWT] Ed25519 verification SUCCESS!");
    }
    
    return valid;
}

bool JWTVerifier::parsePayload(const String& payloadJson, JWTPayload& outPayload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadJson);
    
    if (error) {
        Serial.print("[JWT] JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    outPayload.card_id = doc["card_id"].as<String>();
    outPayload.card_uid = doc["card_uid"] | "";
    outPayload.user_id = doc["user_id"] | "";
    outPayload.access_level = doc["access_level"] | "staff";
    outPayload.exp = doc["exp"] | 0;
    outPayload.offline_max_until = doc["offline_max_until"] | 0;
    outPayload.valid = true;
    
    // Validate required fields
    if (outPayload.card_id.length() == 0 || outPayload.exp == 0) {
        Serial.println("[JWT] Missing required fields");
        outPayload.valid = false;
        return false;
    }
    
    return true;
}
