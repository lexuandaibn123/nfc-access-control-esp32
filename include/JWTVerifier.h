#ifndef JWTVERIFIER_H
#define JWTVERIFIER_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct JWTPayload {
    String card_id;
    String card_uid;
    String user_id;
    String access_level;
    unsigned long exp;
    unsigned long offline_max_until;
    bool valid;
};

class JWTVerifier {
public:
    JWTVerifier();
    
    // Verify JWT signature and extract payload
    bool verify(const String& jwt, const String& publicKeyPem, JWTPayload& outPayload);
    
private:
    int base64UrlDecode(const String& input, uint8_t* output, int maxLen);
    bool verifyEdDSASignature(const String& message, const String& signature, const String& publicKeyPem);
    bool parsePayload(const String& payloadJson, JWTPayload& outPayload);
};

#endif
